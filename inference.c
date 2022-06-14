#include <errno.h>
#include <stdlib.h>	   // strtol
#include <string.h>	   // strcmp

#include "inference.h"
#include "context.h"

#define MAX_VARS 20 /* Max vars in same context, used in fresh & freshrec */

typedef struct TypeList {
	Type *val;
	struct TypeList *next;
} TypeList;

static Type *
prune(Type *t)
{
	if(t->type != VARIABLE)
		return t;
	if(t->instance == NULL)
		return t;
	t->instance = prune(t->instance);
	return t->instance;
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

typedef struct _mp_item {
	Type *from;
	Type *to;
	struct _mp_item *next;
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
freshrec(Inferencer *ctx, Type *tp, TypeList *ngs, TypeMap *map)
{
	Type *p = prune(tp);
	switch(p->type) {
	case VARIABLE: {
		if(is_generic(p, ngs))
			return find_or_add(ctx, map, p);
		return p;
	}
	case OPERATOR: {
		Type *ret = make_type(ctx);
		*ret = (Type){.type = OPERATOR,
			      .op_name = p->op_name,
			      .args = p->args};
		for(int i = 0; i < ret->args; ++i) {
			ret->types[i] = freshrec(ctx, p->types[i], ngs, map);
		}
		return ret;
	}
	default:
		return Err(ctx, UNIFY_ERROR, NULL);
	}
}

static Type *
fresh(Inferencer *ctx, Type *t, TypeList *ngs)
{
	TypeMap map[MAX_VARS];
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
	long l = strtol(name, NULL, 0);
	if(l != 0 || (l == 0 && errno != EINVAL)) {
		return Integer(ctx);
	}
	Env *cur = env;
	while(cur != NULL) {
		if(!strcmp(name, cur->name))
			return fresh(ctx, cur->node, ngs);
		cur = cur->next;
	}
	return Err(ctx, UNDEFINED_SYMBOL, name);
}

static Type *
unify(Inferencer *ctx, Type *t1, Type *t2)
{
	Type *a = prune(t1);
	Type *b = prune(t2);

	switch(a->type) {
	case VARIABLE:
		if(a == b)
			return a;
		if(occurs_in_type(a, b))
			return Err(ctx, RECURSIVE_UNIFICATION, NULL);
		a->instance = b;
		return a;
	case OPERATOR:
		if(b->type == VARIABLE)
			return unify(ctx, b, a);
		if(b->type == OPERATOR) {
			if(strcmp(a->op_name, b->op_name) || a->args != b->args)
				Err(ctx, TYPE_MISMATCH, NULL);
			for(int i = 0; i < a->args; ++i)
				(void)unify(ctx, a->types[i], b->types[i]);
			return a;
		}
	default:
		if(a->type < 0)
			return a;
		if(b->type < 0)
			return b;
		return Err(ctx, UNIFY_ERROR, NULL);
	}
}

Type *
analyze(Inferencer *ctx, Term *node, Env *env, TypeList *ngs)
{
	switch(node->type) {
	case IDENTIFIER:
		return get_type(ctx, node->name, env, ngs);
	case APPLY: {
		Type *func = analyze(ctx, node->fn, env, ngs);
		if(func->type < 0)
			return func;
		Type *arg = analyze(ctx, node->arg, env, ngs);
		if(arg->type < 0)
			return arg;
		Type *res = Var(ctx);
		if(res < 0)
			return res;
		(void)unify(ctx, Function(ctx, arg, res), func);
		return res;
	}
	case LAMBDA: {
		Type *arg = Var(ctx);
		if(arg->type < 0)
			return arg;
		Env new_env = {.name = node->v,
			       .node = arg,
			       .next = env};
		TypeList new_ngs = {.val = arg, .next = ngs};
		Type *res = analyze(ctx, node->body, &new_env, &new_ngs);
		if(res->type < 0)
			return res;
		return Function(ctx, arg, res);
	}
	case LET: {
		Type *defn = analyze(ctx, node->defn, env, ngs);
		if(defn->type < 0)
			return defn;
		Env new_env = {.name = node->v,
			       .node = defn,
			       .next = env};
		return analyze(ctx, node->body, &new_env, ngs);
	}
	case LETREC: {
		Type *new = Var(ctx);
		if(new->type < 0)
			return new;
		Env new_env = {.name = node->v,
			       .node = new,
			       .next = env};
		TypeList new_ng = {.val = new, .next = ngs};
		Type *defn = analyze(ctx, node->defn, &new_env, &new_ng);
		if(defn->type < 0)
			return defn;
		(void)unify(ctx, new, defn);
		return analyze(ctx, node->body, &new_env, ngs);
	}
	default:
		return Err(ctx, UNHANDLED_SYNTAX_NODE, NULL);
	}
}
