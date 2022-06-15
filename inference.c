#include <errno.h>
#include <stdlib.h>	   // strtol
#include <string.h>	   // strcmp

#include "inference.h"

#define MAX_VARS 20 /* Max vars in same context, used in fresh & freshrec */

typedef struct TypeList {
	Type *val;
	struct TypeList *next;
} TypeList;
typedef struct TypeMap {
	Type *from;
	Type *to;
	struct TypeMap *next;
} TypeMap;
static Type *
find_or_add(Inferencer *ctx, TypeMap *map, Type *p)
{
	TypeMap *cur = map;
	while(cur != NULL && cur->from != NULL) {
		if(cur->from == p) {
			return cur->to;
		}
		cur = cur->next;
	}
	if(cur == NULL)
		return Err(ctx, LOCAL_SCOPE_EXCEEDED, NULL);
	cur->from = p;
	cur->to = Var(ctx);
	return cur->to;
}

static Type *
prune(Type *t)
{
	while(t && t->type == VARIABLE && t->instance)
		t = t->instance;
	return t;
}
static int
occurs_in_type(Type *v, Type *type2)
{
	Type *pruned_type2 = prune(type2);
	if(pruned_type2 == v)
		return 1;
	if(pruned_type2->type == OPERATOR)
		for(int i = 0; i < pruned_type2->args; ++i)
			if(occurs_in_type(v, pruned_type2->types[i]))
				return 1;
	return 0;
}
static int
is_generic(Type *v, TypeList *ngs)
{
	/* Flip the return value because we're checking ngss for a generic */
	while(ngs != NULL) {
		if(occurs_in_type(v, ngs->val))
			return 0;
		ngs = ngs->next;
	}
	return 1;
}

static Type *
freshrec(Inferencer *ctx, Type *tp, TypeList *ngs, TypeMap *map)
{
	Type *p = prune(tp);
	if(ctx->error)
		return NULL;
	switch(p->type) {
	case VARIABLE: {
		if(is_generic(p, ngs))
			return find_or_add(ctx, map, p);
		else
			return p;
	}
	case OPERATOR: {
		Type *ret = make_type(ctx);
		*ret = (Type){.type = OPERATOR,
			      .name = p->name,
			      .args = p->args};
		for(int i = 0; i < ret->args; ++i) {
			ret->types[i] = freshrec(ctx, p->types[i], ngs, map);
		}
		return ret;
	}
	}
}
static Type *
fresh(Inferencer *ctx, Type *t, TypeList *ngs)
{
	TypeMap map[MAX_VARS];
	if(ctx->error)
		return NULL;
	for(int i = 0; i < MAX_VARS - 1; ++i) {
		map[i].next = &map[i + 1];
	}
	map[MAX_VARS - 1].next = NULL;
	for(int i = 0; i < MAX_VARS; ++i) {
		map[i].from = map[i].to = NULL;
	}
	return freshrec(ctx, t, ngs, map);
}
static Type *
get_type(Inferencer *ctx, char *name, Env *env, TypeList *ngs)
{
	Env *cur = env;
	long l = strtol(name, NULL, 0);
	if(ctx->error)
		return NULL;
	if(l != 0 || (l == 0 && errno != EINVAL)) {
		return Integer(ctx);
	}
	while(cur != NULL) {
		if(!strcmp(name, cur->name))
			return fresh(ctx, cur->node, ngs);
		cur = cur->next;
	}
	return Err(ctx, UNDEFINED_SYMBOL, name);
}
static void
unify(Inferencer *ctx, Type *t1, Type *t2)
{
	Type *a = prune(t1);
	Type *b = prune(t2);
	if(ctx->error) /* Neither a nor b are NULL subsequently */
		return;
	if(a->type == OPERATOR && b->type == VARIABLE) { /* Normalize */
		Type *swp = a;
		a = b;
		b = swp;
	}
	switch(a->type) {
	case VARIABLE:
		if(a == b)
			return;
		else if(!occurs_in_type(a, b))
			a->instance = b;
		else
			Err(ctx, RECURSIVE_UNIFICATION, NULL);
		break;
	case OPERATOR:
		if(strcmp(a->name, b->name) || a->args != b->args)
			Err(ctx, TYPE_MISMATCH, NULL);
		else
			for(int i = 0; i < a->args; ++i)
				unify(ctx, a->types[i], b->types[i]);
		break;
	}
	return;
}
static Type *
analyze(Inferencer *ctx, Term *node, Env *env, TypeList *ngs)
{
	if(ctx->error)
		return NULL;
	switch(node->type) {
	case IDENTIFIER:
		return get_type(ctx, node->name, env, ngs);
	case APPLY: {
		Type *func = analyze(ctx, node->fn, env, ngs);
		Type *arg = analyze(ctx, node->arg, env, ngs);
		Type *res = Var(ctx);
		unify(ctx, Function(ctx, arg, res), func);
		return res;
	}
	case LAMBDA: {
		Type *arg = Var(ctx);
		Env new_env = {.name = node->v,
			       .node = arg,
			       .next = env};
		TypeList new_ngs = {.val = arg, .next = ngs};
		Type *res = analyze(ctx, node->body, &new_env, &new_ngs);
		return Function(ctx, arg, res);
	}
	case LET: {
		Type *defn = analyze(ctx, node->defn, env, ngs);
		Env new_env = {.name = node->v,
			       .node = defn,
			       .next = env};
		return analyze(ctx, node->body, &new_env, ngs);
	}
	case LETREC: {
		Type *new = Var(ctx);
		Env new_env = {.name = node->v,
			       .node = new,
			       .next = env};
		TypeList new_ng = {.val = new, .next = ngs};
		Type *defn = analyze(ctx, node->defn, &new_env, &new_ng);
		unify(ctx, new, defn);
		return analyze(ctx, node->body, &new_env, ngs);
	}
	default:
		return Err(ctx, UNHANDLED_SYNTAX_NODE, NULL);
	}
}
error_t
extern_analyze(Inferencer *ctx, Term *node, Env *env, TypeList *ngs)
{
	ctx->result = analyze(ctx, node, env, ngs);
	if(ctx->error)
		return ctx->error;
	else
		return OK;
}
