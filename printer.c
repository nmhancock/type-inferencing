#include <stdio.h>
#include <stdlib.h>

#include "inference.h"

static char *
print_a_type(struct lang_type *t)
{
	char *ret;
	if(!t) {
		asprintf(&ret, "%s\n", "NULL");
		return ret;
	}
	switch(t->type) {
	case VARIABLE:
		if(!t->instance) {
			asprintf(&ret, "%s (%d)", t->var_name, t->id);
			return ret;
		}
		char *instance = print_a_type(t->instance);
		if(instance)
			return instance;
		asprintf(&ret, "NULL");
		return ret;
	case OPERATOR:
		/* Ensure caller can free as expected */
		if(t->args == 0)
			asprintf(&ret, "%s", t->op_name);
		else if(t->args == 2) {
			char *type0 = print_a_type(t->types[0]);
			char *type1 = print_a_type(t->types[1]);
			asprintf(&ret, "(%s %s %s)", type0 ? type0 : "NULL",
				 t->op_name, type1 ? type1 : "NULL");
			free(type0);
			free(type1);
		} else /* TODO: Implement properly, don't be lazy! */
			asprintf(&ret, "%s", t->op_name);
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

static void
print_type(struct lang_type *t)
{
	char *res = print_a_type(t);
	if(!res)
		printf("NULL\n");
	else
		printf("%s\n", res);
	free(res);
}

static char *
print_ast_node(struct ast_node *n)
{
	char *ret;
	if(!n) {
		asprintf(&ret, "%s\n", "NULL");
		return ret;
	}
	switch(n->type) {
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

static void
print_ast(struct ast_node *n)
{
	char *res = print_ast_node(n);
	if(!res)
		printf("NULL\n");
	else
		printf("%s\n", res);
	free(res);
}

void
print(struct ast_node *n, struct lang_type *t)
{
	char *ast = print_ast_node(n);
	char *type = print_a_type(t);
	printf("%s : %s\n", ast, type);
	free(type);
	free(ast);
}
