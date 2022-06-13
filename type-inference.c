#include <errno.h>
#include <stdlib.h> // strtol
#include <string.h> // strcmp

#include "type-inference.h"

#define MAX_VARS 20 /* Max vars in same context, used in fresh & freshrec */

struct lt_list {
	struct lang_type *val;
	struct lt_list *next;
};

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

static struct lang_type *freshrec(struct inferencing_ctx *ctx,
				  struct lang_type *tp, struct lt_list *ngs,
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

static struct lang_type *fresh(struct inferencing_ctx *ctx, struct lang_type *t,
			       struct lt_list *ngs)
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

static struct lang_type *get_type(struct inferencing_ctx *ctx, char *name,
				  struct env *env, struct lt_list *ngs)
{
	long l = strtol(name, NULL, 0);
	if (l != 0 || (l == 0 && errno != EINVAL)) {
		return Integer(ctx);
	}
	struct env *cur = env;
	while (cur != NULL) {
		if (!strcmp(name, cur->name))
			return fresh(ctx, cur->node, ngs);
		cur = cur->next;
	}
	return Err(ctx, UNDEFINED_SYMBOL, name);
}

static struct lang_type *unify(struct inferencing_ctx *ctx,
			       struct lang_type *t1, struct lang_type *t2)
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

struct lang_type *analyze(struct inferencing_ctx *ctx, struct ast_node *node,
			  struct env *env, struct lt_list *ngs)
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
		(void)unify(ctx, Function(ctx, arg_type, result_type),
			    fun_type);
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
		struct lang_type *defn_type =
			analyze(ctx, node->defn, env, ngs);
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
