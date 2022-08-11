#include <errno.h>
#include <stdlib.h>  /* strtol */
#include <string.h>  /* strcmp */
#include <stddef.h>  /* offsetof */
#include <stdbool.h> /* true, false */

#include "inference.h"

#define MAX_DEPTH 40

/* Gets the child pointer */
static Term *
get_child(Term *t, ssize_t offset)
{
	if(offset == -1)
		return NULL;
	return *(Term **)((uintptr_t)t + offset);
}

typedef struct TData {
	Type *type;
	char *name;
} TData;
typedef struct TStack {
	TData stack[MAX_DEPTH];
	size_t use;
	size_t cap;
} TStack;

static Type *
lookup(TStack *types, size_t start, size_t end, char *name)
{
	for(size_t i = start; i < end; ++i)
		if(types->stack[i].name && !strcmp(name, types->stack[i].name))
			return types->stack[i].type;
	return NULL;
}

static void
populate_definitions(TStack *types, size_t locals, char *except)
{
	for(size_t i = locals; i < types->use; ++i) {
		TData *cur = &types->stack[i];
		Type *found = lookup(types, 0, locals, cur->name);
		if(found && (!except || (except && strcmp(cur->name, except))))
			cur->type = found;
	}
}

static void
reverse(TStack *types, size_t start, size_t end)
{
	for(size_t low = start, high = end - 1; low < high; low++, high--) {
		TData tmp = types->stack[low];
		types->stack[low] = types->stack[high];
		types->stack[high] = tmp;
	}
}

static void
simplify(Inferencer *ctx, TStack *types, size_t locals)
{
	for(size_t i = locals, applies = 0, end = types->use; i < end; i++) {
		Type *cur = types->stack[i].type;
		if(cur == Apply(ctx)) {
			applies++;
			continue;
		}
		if(cur->type == VARIABLE && cur->instance)
			cur = cur->instance;
		/* simplify condition below */
		while(types->use - end > 0 && applies > 0 && cur->type == OPERATOR && cur->args > 0) {
			Type *prev = types->stack[types->use-- - 1].type;
			Type *left = cur->types[0];
			Type *right = cur->types[1];
			if(prev->type == VARIABLE && prev->instance == NULL) {
				if(prev->instance == NULL)
					prev->instance = left;
				prev = prev->instance;
			}
			while(left->type == VARIABLE && left->instance)
				left = left->instance;
			while(right->type == VARIABLE && right->instance)
				right = right->instance;
			if(left->type == VARIABLE) {
				left->instance = prev;
				left = left->instance;
			}
			if(prev != left)
				Err(ctx, TYPE_MISMATCH, "");
			else
				cur = right;
			applies--;
		}
		types->stack[types->use++] = (TData){cur, ""};
	}
	return;
}

static void
analyze_type(Inferencer *ctx, TStack *types, Term *t)
{
	switch(t->type) {
	case IDENTIFIER: {
		/* Lookup name in local scope _only_ since we don't
		 * know which, if any, global variables are shadowed
		 */
		Type *v = lookup(types, ctx->locals, types->use, t->name);
		long l = strtol(t->name, NULL, 0);
		if(l != 0 || (l == 0 && errno != EINVAL))
			v = Integer(ctx);
		else if(!v) {
			v = Var(ctx);
			v->name = t->name;
		}
		types->stack[types->use++] = (TData){v, t->name};
		break;
	}
	case APPLY:
		types->stack[types->use++] = (TData){Apply(ctx), "_apply"};
		break;
	case LAMBDA: {
		Type *arg = lookup(types, ctx->locals, types->use, t->v);
		populate_definitions(types, ctx->locals, t->v); /* resolve */
		reverse(types, ctx->locals, types->use);
		(void)simplify(ctx, types, ctx->locals);
		types->stack[ctx->locals++] = (TData){
			Function(ctx, arg, types->stack[types->use-- - 1].type),
			""};
		types->use = ctx->locals;
		break;
	}
	case LET:
	case LETREC:
		types->stack[ctx->locals - 1].name = t->v; /* name let var */
		populate_definitions(types, ctx->locals, NULL);
		reverse(types, ctx->locals, types->use);
		simplify(ctx, types, ctx->locals);
		break;
	}
}

error_t
analyze(Inferencer *ctx, Term *root, Env *env)
{
	typedef struct PData {
		Term *term;
		size_t seen;
	} PData;
	typedef struct PStack {
		PData stack[MAX_DEPTH];
		size_t use;
		size_t cap;
	} PStack;
	PStack terms = {{{NULL}, {0}}, 0, MAX_DEPTH};
	TStack types = {{{NULL, NULL}}, 0, MAX_DEPTH};
	/* maps from type and seen to offset of child pointer */
	ssize_t child_offsets[5][3] = {
		[IDENTIFIER] = {-1},
		[APPLY] = {offsetof(Term, fn), offsetof(Term, arg), -1},
		[LAMBDA] = {offsetof(Term, body), -1},
		[LET] = {offsetof(Term, defn), offsetof(Term, body), -1},
		[LETREC] = {offsetof(Term, defn), offsetof(Term, body), -1},
	};
	while(env != NULL) {
		types.stack[types.use++] = (TData){env->node, env->name};
		env = env->next;
	}
	ctx->locals = types.use;
	if(terms.use == terms.cap)
		return OUT_OF_TYPES;
	if(root)
		terms.stack[terms.use++] = (PData){.term = root, .seen = 0};
	while(terms.use > 0) {
		Term *term = terms.stack[terms.use - 1].term;
		size_t seen = terms.stack[terms.use-- - 1].seen;
		Term *child = get_child(term, child_offsets[term->type][seen]);
		if(child) {
			terms.stack[terms.use++] = (PData){term, seen + 1};
			terms.stack[terms.use++] = (PData){child, 0};
			if(terms.use == terms.cap)
				return MAX_RECURSION_EXCEEDED;
		} else
			analyze_type(ctx, &types, term);
	}
	ctx->result = types.stack[types.use - 1].type;
	return OK;
}
