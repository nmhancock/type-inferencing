#include <stdio.h>
#include <stdlib.h>

#include "inference.h"

static char *
print_a_type(Type *t)
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
	default:
		asprintf(&ret, "Unexpected Type: %d", t->type);
		return ret;
	}
}

static char *
print_an_error(error_t e, char *symbol)
{
	char *ret;
	switch(e) {
	case OK:
		asprintf(&ret, "No error");
		break;
	case UNHANDLED_SYNTAX_NODE:
		asprintf(&ret, "Unhandled syntax node");
		break;
	case UNDEFINED_SYMBOL:
		asprintf(&ret, "Undefined symbol %s\n", symbol);
		break;
	case RECURSIVE_UNIFICATION:
		asprintf(&ret, "Recursive unification");
		break;
	case TYPE_MISMATCH:
		asprintf(&ret, "Type mismatch");
		break;
	case UNIFY_ERROR:
		asprintf(&ret, "Unification error");
		break;
	case OUT_OF_TYPES:
		asprintf(&ret, "Out of types!");
		break;
	case LOCAL_SCOPE_EXCEEDED:
		asprintf(&ret, "Too many variables in local scope");
		break;
	}
	return ret;
}

static char *
print_term(Term *n)
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
		char *fn = print_term(n->fn);
		char *arg = print_term(n->arg);
		asprintf(&ret, "(%s %s)", fn ? fn : "NULL", arg ? arg : "NULL");
		free(fn);
		free(arg);
		return ret;
	}
	case LAMBDA: {
		char *body = print_term(n->body);
		asprintf(&ret, "(fn %s => %s)", n->v, body ? body : "NULL");
		free(body);
		return ret;
	}
	case LET: {
		char *body = print_term(n->body);
		char *defn = print_term(n->defn);
		asprintf(&ret, "(let %s = %s in %s)", n->v,
			 defn ? defn : "NULL", body ? body : "NULL");
		free(defn);
		free(body);
		return ret;
	}
	case LETREC: {
		char *body = print_term(n->body);
		char *defn = print_term(n->defn);
		asprintf(&ret, "(letrec %s = %s in %s)", n->v,
			 defn ? defn : "NULL", body ? body : "NULL");
		free(defn);
		free(body);
		return ret;
	}
	}
	return ret;
}

void
print(Term *n, Type *t)
{
	char *ast = print_term(n);
	char *type = print_a_type(t);
	printf("%s : %s\n", ast ? ast : "NULL", type ? type : "NULL");
	free(type);
	free(ast);
}

void
print_error(Term *n, error_t e, char *symbol)
{
	char *ast = print_term(n);
	char *err = print_an_error(e, symbol);
	printf("%s : %s\n", ast ? ast : "NULL", err ? err : "NULL");
	free(err);
	free(ast);
}
