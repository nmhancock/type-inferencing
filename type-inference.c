#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "type-inference.h"

#define MAX_VARS 20 /* Max vars in same context, used in fresh & freshrec */
#define MAX_TYPES 200 /* Max total types, used in global below */

struct inferencing_ctx {
	struct lang_type *types;
	int current_type;
};
struct lt_list {
	struct lang_type *val;
	struct lt_list *next;
};

/* Temporary globals */
struct lang_type *Integer;
struct lang_type *Bool;

/* Debugging functions */
static void print_ast(struct ast_node *n);
static void print_type(struct lang_type *t);

static struct lang_type *make_type(struct inferencing_ctx* ctx)
{
	assert(ctx->current_type < MAX_TYPES);
	ctx->types[ctx->current_type].id = ctx->current_type;
	return &ctx->types[ctx->current_type++];
}

static struct lang_type *Var(struct inferencing_ctx* ctx)
{
	struct lang_type *result_type = make_type(ctx);
	*result_type = (struct lang_type){
		.type = VARIABLE,
		.instance = NULL,
		.var_name = NULL,
	};
	return result_type;
}

static char *fname = "->";
static struct lang_type *Function(struct inferencing_ctx* ctx, struct lang_type *arg_t,
				  struct lang_type *res_t)
{
	struct lang_type *function = make_type(ctx);
	/* Unrolling the constructor to keep the same form as the old code. */
	*function = (struct lang_type){
		.type = OPERATOR,
		.op_name = fname,
		.args = 2,
		.types = { arg_t, res_t },
	};
	return function;
}

static struct lang_type *Err(struct inferencing_ctx* ctx, enum lang_type_type err, char *symbol)
{
	struct lang_type *error = make_type(ctx);
	*error = (struct lang_type){ .type = err, .undefined_symbol = symbol };
	return error;
}

static struct lang_type *prune(struct lang_type *t)
{
	if (t->type != VARIABLE)
		return t;
	if (t->instance == NULL)
		return t;
	t->instance = prune(t->instance);
	return t->instance;
}

static int occurs_in_type(struct lang_type *v, struct lang_type *type2)
{
	struct lang_type *pruned_type2 = prune(type2);
	if (pruned_type2 == v)
		return 1;
	if (pruned_type2->type == OPERATOR)
		for (int i = 0; i < pruned_type2->args; ++i)
			if (occurs_in_type(v, pruned_type2->types[i]))
				return 1;
	return 0;
}

static int is_generic(struct lang_type *v, struct lt_list *ngs)
{
	/* Flip the return value because we're checking ngss for a generic */
	while (ngs != NULL) {
		if (occurs_in_type(v, ngs->val))
			return 0;
		ngs = ngs->next;
	}
	return 1;
}

struct _mp_item {
	struct lang_type *from;
	struct lang_type *to;
	struct _mp_item *next;
};

static struct lang_type *freshrec(struct inferencing_ctx* ctx, struct lang_type *tp, struct lt_list *ngs,
				  struct _mp_item *map)
{
	struct lang_type *p = prune(tp);
	switch (p->type) {
	case VARIABLE: {
		if (!is_generic(p, ngs))
			return p;
		struct _mp_item *cur = map;
		while (cur != NULL && cur->from != NULL) {
			if (cur->from == p) {
				return cur->to;
			}
			cur = cur->next;
		}
		if (cur == NULL)
			return Err(ctx, LOCAL_SCOPE_EXCEEDED, NULL);
		cur->from = p;
		cur->to = Var(ctx);
		return cur->to;
	}
	case OPERATOR: {
		struct lang_type *ret = make_type(ctx);
		*ret = (struct lang_type){ .type = OPERATOR,
					   .op_name = p->op_name,
					   .args = p->args };
		for (int i = 0; i < ret->args; ++i) {
			ret->types[i] = freshrec(ctx, p->types[i], ngs, map);
		}
		return ret;
	}
	default:
		return Err(ctx, UNIFY_ERROR, NULL);
	}
};

static struct lang_type *fresh(struct inferencing_ctx* ctx, struct lang_type *t, struct lt_list *ngs)
{
	struct _mp_item map[MAX_VARS];
	for (int i = 0; i < MAX_VARS - 1; ++i) {
		map[i].next = &map[i + 1];
	}
	map[MAX_VARS - 1].next = NULL;
	for (int i = 0; i < MAX_VARS; ++i) {
		map[i].from = map[i].to = NULL;
	}
	return freshrec(ctx, t, ngs, map);
}

static struct lang_type *get_type(struct inferencing_ctx* ctx, char *name, struct env *env,
				  struct lt_list *ngs)
{
	long l = strtol(name, NULL, 0);
	if (l != 0 || l == 0 && errno != EINVAL) {
		return Integer;
	}
	struct env *cur = env;
	while (cur != NULL) {
		if (!strcmp(name, cur->name))
			return fresh(ctx, cur->node, ngs);
		cur = cur->next;
	}
	return Err(ctx, UNDEFINED_SYMBOL, name);
}

static struct lang_type *unify(struct inferencing_ctx* ctx, struct lang_type *t1, struct lang_type *t2)
{
	struct lang_type *a = prune(t1);
	struct lang_type *b = prune(t2);

	switch (a->type) {
	case VARIABLE:
		if (a == b)
			return a;
		if (occurs_in_type(a, b))
			return Err(ctx, RECURSIVE_UNIFICATION, NULL);
		a->instance = b;
		return a;
	case OPERATOR:
		if (b->type == VARIABLE)
			return unify(ctx, b, a);
		if (b->type == OPERATOR) {
			if (strcmp(a->op_name, b->op_name) ||
			    a->args != b->args)
				Err(ctx, TYPE_MISMATCH, NULL);
			for (int i = 0; i < a->args; ++i)
				(void)unify(ctx, a->types[i], b->types[i]);
			return a;
		}
	default:
		if (a->type < 0)
			return a;
		if (b->type < 0)
			return b;
		return Err(ctx, UNIFY_ERROR, NULL);
	}
}

struct lang_type *analyze(struct inferencing_ctx* ctx, struct ast_node *node, struct env *env,
			  struct lt_list *ngs)
{
	switch (node->type) {
	case IDENTIFIER:
		return get_type(ctx, node->name, env, ngs);
	case APPLY: {
		struct lang_type *fun_type = analyze(ctx, node->fn, env, ngs);
		if (fun_type->type < 0)
			return fun_type;
		struct lang_type *arg_type = analyze(ctx, node->arg, env, ngs);
		if (arg_type->type < 0)
			return arg_type;
		struct lang_type *result_type = Var(ctx);
		(void)unify(ctx, Function(ctx, arg_type, result_type), fun_type);
		return result_type;
	}
	case LAMBDA: {
		struct lang_type *arg_type = Var(ctx);
		struct env new_env = { .name = node->v,
				       .node = arg_type,
				       .next = env };
		struct lt_list new_ng = { .val = arg_type, .next = ngs };
		struct lang_type *result_type =
			analyze(ctx, node->body, &new_env, &new_ng);
		if (result_type->type < 0)
			return result_type;
		return Function(ctx, arg_type, result_type);
	}
	case LET: {
		struct lang_type *defn_type = analyze(ctx, node->defn, env, ngs);
		if (defn_type->type < 0)
			return defn_type;
		struct env new_env = { .name = node->v,
				       .node = defn_type,
				       .next = env };
		return analyze(ctx, node->body, &new_env, ngs);
	}
	case LETREC: {
		struct lang_type *new_type = Var(ctx);
		struct env new_env = { .name = node->v,
				       .node = new_type,
				       .next = env };
		struct lt_list new_ng = { .val = new_type, .next = ngs };
		struct lang_type *defn_type =
			analyze(ctx, node->defn, &new_env, &new_ng);
		if (defn_type->type < 0)
			return defn_type;
		(void)unify(ctx, new_type, defn_type);
		return analyze(ctx, node->body, &new_env, ngs);
	}
	default:
		return Err(ctx, UNHANDLED_SYNTAX_NODE, NULL);
	}
}

/* Usage examples */
#include <stdio.h>
#include <time.h>

char *print_a_type(struct lang_type *t)
{
	char *ret;
	if (!t) {
		asprintf(&ret, "%s\n", "NULL");
		return ret;
	}
	switch (t->type) {
	case VARIABLE:
		if (!t->instance) {
			asprintf(&ret, "%s (%d)", t->var_name, t->id);
			return ret;
		}
		char *instance = print_a_type(t->instance);
		if (instance)
			return instance;
		asprintf(&ret, "NULL");
		return ret;
	case OPERATOR:
		/* Ensure caller can free as expected */
		if (t->args == 0)
			asprintf(
				&ret, "%s",
				t->op_name);
		else if (t->args == 2) {
			char *type0 = print_a_type(t->types[0]);
			char *type1 = print_a_type(t->types[1]);
			asprintf(&ret, "(%s %s %s)", type0 ? type0 : "NULL",
				 t->op_name, type1 ? type1 : "NULL");
			free(type0);
			free(type1);
		} else /* TODO: Implement properly, don't be lazy! */
			asprintf(
				&ret, "%s",
				t->op_name);
		return ret;
	case UNHANDLED_SYNTAX_NODE:
		asprintf(&ret, "Unhandled syntax node");
		return ret;
	case UNDEFINED_SYMBOL:
		asprintf(&ret, "Undefined symbol %s\n", t->undefined_symbol);
		return ret;
	case RECURSIVE_UNIFICATION:
		asprintf(&ret, "Recursive unification");
		return ret;
	case TYPE_MISMATCH:
		asprintf(&ret, "Type mismatch");
		return ret;
	case UNIFY_ERROR:
		asprintf(&ret, "Unification error");
		return ret;
	default:
		asprintf(&ret, "Unexpected Type: %d", t->type);
		return ret;
	}
}

void print_type(struct lang_type *t)
{
	char *res = print_a_type(t);
	if (!res)
		printf("NULL\n");
	else
		printf("%s\n", res);
	free(res);
}

char *print_ast_node(struct ast_node *n)
{
	char *ret;
	if (!n) {
		asprintf(&ret, "%s\n", "NULL");
		return ret;
	}
	switch (n->type) {
	case IDENTIFIER: {
		asprintf(&ret, "%s", n->name ? n->name : "NULL");
		return ret;
	}
	case APPLY: {
		char *fn = print_ast_node(n->fn);
		char *arg = print_ast_node(n->arg);
		asprintf(&ret, "(%s %s)", fn ? fn : "NULL", arg ? arg : "NULL");
		free(fn);
		free(arg);
		return ret;
	}
	case LAMBDA: {
		char *body = print_ast_node(n->body);
		asprintf(&ret, "(fn %s => %s)", n->v, body ? body : "NULL");
		free(body);
		return ret;
	}
	case LET: {
		char *body = print_ast_node(n->body);
		char *defn = print_ast_node(n->defn);
		asprintf(&ret, "(let %s = %s in %s)", n->v,
			 defn ? defn : "NULL", body ? body : "NULL");
		free(defn);
		free(body);
		return ret;
	}
	case LETREC: {
		char *body = print_ast_node(n->body);
		char *defn = print_ast_node(n->defn);
		asprintf(&ret, "(letrec %s = %s in %s)", n->v,
			 defn ? defn : "NULL", body ? body : "NULL");
		free(defn);
		free(body);
		return ret;
	}
	}
	return ret;
}

void print_ast(struct ast_node *n)
{
	char *res = print_ast_node(n);
	if (!res)
		printf("NULL\n");
	else
		printf("%s\n", res);
	free(res);
}

void print(struct ast_node *n, struct lang_type *t)
{
	char *ast = print_ast_node(n);
	char *type = print_a_type(t);
	printf("%s : %s\n", ast, type);
	free(type);
	free(ast);
}

int main(void)
{
        struct lang_type types[MAX_TYPES];
        struct inferencing_ctx ctx = { .types = &types, .current_type = 2 };
        Integer = &types[0];
        Bool = &types[1];

	/* Basic types are constructed with a nullary type constructor */
	*Integer = (struct lang_type){
		.type = OPERATOR, .op_name = "int", .types = NULL, .args = 0
	};
	*Bool = (struct lang_type){
		.type = OPERATOR, .op_name = "bool", .types = NULL, .args = 0
	};

	struct lang_type *var1 = Var(&ctx);
	struct lang_type *var2 = Var(&ctx);
	struct lang_type *pair_type = make_type(&ctx);
	*pair_type = (struct lang_type){ .type = OPERATOR,
					 .op_name = "*",
					 .args = 2,
					 .types = { var1, var2 } };
	struct lang_type *var3 = Var(&ctx);

	struct env envs[7] = {
		{ .name = "pair",
		  .node = Function(&ctx, var1, Function(&ctx, var2, pair_type)),
		  .next = &envs[1] },
		{ .name = "true", .node = Bool, .next = &envs[2] },
		{
			.name = "cond",
			.node = Function(&ctx, Bool,
					 Function(&ctx, var3, Function(&ctx, var3, var3))),
			.next = &envs[3],
		},
		{ .name = "zero",
		  .node = Function(&ctx, Integer, Bool),
		  .next = &envs[4] },
		{ .name = "pred",
		  .node = Function(&ctx, Integer, Integer),
		  .next = &envs[5] },
		{ .name = "times",
		  .node = Function(&ctx, Integer, Function(&ctx, Integer, Integer)),
		  .next = &envs[6] },
		{ .name = "factorial",
		  .node = Function(&ctx, Integer, Integer),
		  .next = NULL }
	};
	struct env *my_env = &envs;

	struct ast_node factorial = { .type = LETREC,
				      .v = "factorial",
				      .defn =
					      &(struct ast_node){
						      .type = LAMBDA,
						      .v = "n",
						      .body =
							      &(struct ast_node){
								      .type = APPLY,
								      .fn =
									      &(
										      struct ast_node){ .type = APPLY,
													.fn =
														&(struct ast_node){ .type = APPLY,
																    .fn =
																	    &(struct ast_node){
																		    .type = IDENTIFIER,
																		    .name = "cond" },
																    .arg =
																	    &(struct ast_node){
																		    .type = APPLY,
																		    .fn = &(struct
																			    ast_node){ .type = IDENTIFIER,
																				       .name = "zero" },
																		    .arg = &(
																			    struct
																			    ast_node){ .type = IDENTIFIER, .name = "n" } } },
													.arg =
														&(struct ast_node){
															.type = IDENTIFIER,
															.name = "1" } },
								      .arg = &(struct ast_node){ .type = APPLY,
												 .fn =
													 &(struct ast_node){
														 .type = APPLY,
														 .fn =
															 &(struct
															   ast_node){ .type = IDENTIFIER,
																      .name = "times" },
														 .arg =
															 &(struct ast_node){
																 .type = IDENTIFIER,
																 .name = "n" },
													 },
												 .arg = &(struct
													  ast_node){ .type = APPLY,
														     .fn = &(struct ast_node){ .type = IDENTIFIER, .name = "factorial" },
														     .arg =
															     &(struct ast_node){
																     .type = APPLY,
																     .fn =
																	     &(struct
																	       ast_node){ .type = IDENTIFIER,
																			  .name = "pred" },
																     .arg =
																	     &(struct ast_node){ .type = IDENTIFIER,
																				 .name = "n" },
															     } } } },
					      },
				      .body = &(struct ast_node){
					      .type = APPLY,
					      .fn =
						      &(struct ast_node){
							      .type = IDENTIFIER,
							      .name = "factorial" },
					      .arg =
						      &(struct ast_node){
							      .type = IDENTIFIER,
							      .name = "5" },
				      } };

	struct lang_type *t;
	clock_t total = 0;
#define ITERATIONS 1000000
	for (int i = 0; i < ITERATIONS; ++i) {
		ctx.current_type = 16; /* Experimentally determined */
		clock_t tic = clock();
		t = analyze(&ctx, &factorial, my_env, NULL);
		clock_t toc = clock();
		total += toc - tic;
	}
	print(&factorial, t);
	fprintf(stdout, "Iterations: %d Total time: %f ns\n", ITERATIONS,
		(double)(total / CLOCKS_PER_SEC * 1000000));
	return 0;
}
