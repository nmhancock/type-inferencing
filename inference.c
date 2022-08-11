#include <errno.h>
#include <stdlib.h> /* strtol */
#include <string.h> /* strcmp */
#include <stddef.h> /* offsetof, uint32_t */
#include <stdint.h> /* uint32_t */

#include "inference.h"

#define MAX_DEPTH 40

typedef struct TStack {
	Type *stack[MAX_DEPTH];
	size_t use;
	size_t cap;
} TStack;

static Type *
lookup(TStack *types, size_t start, size_t end, char *name)
{
	for(size_t i = end - 1; i > start; --i)
		if(types->stack[i - 1]->name && !strcmp(name, types->stack[i - 1]->name))
			return types->stack[i - 1];
	return NULL;
}

static void
populate_definitions(Inferencer *ctx, TStack *types, size_t locals, char *except)
{
	for(size_t i = locals; i < types->use; ++i) {
		Type **cur = &types->stack[i];
		Type *found = lookup(types, 0, locals, (*cur)->name);
		if(found && (!except || (except && strcmp((*cur)->name, except))))
			*cur = copy_generic(ctx, found);
	}
}

static void
reverse(TStack *types, size_t start, size_t end)
{
	for(size_t low = start, high = end - 1; low < high; low++, high--) {
		Type *tmp = types->stack[low];
		types->stack[low] = types->stack[high];
		types->stack[high] = tmp;
	}
}

static void
simplify(Inferencer *ctx, TStack *types, size_t locals)
{
	for(size_t i = locals, applies = 0, end = types->use; i < end; i++) {
		Type *cur = types->stack[i];
		if(cur == Apply(ctx)) {
			applies++;
			continue;
		}
		while(types->use > end && applies > 0 && cur->type == OPERATOR && cur->args > 0) {
			Type *prev = types->stack[types->use-- - 1];
			Type *left = cur->types[0];
			Type *right = cur->types[1];
			if(prev->type == VARIABLE) {
				var_is(ctx, prev, left);
				prev = left;
			}
			if(left->type == VARIABLE) {
				var_is(ctx, left, prev);
			}
			if(prev == left || prev->id == left->id)
				cur = right;
			else
				Err(ctx, TYPE_MISMATCH, "");
			applies--;
		}
		types->stack[types->use++] = cur;
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
		types->stack[types->use++] = v;
		break;
	}
	case APPLY:
		types->stack[types->use] = Apply(ctx);
		types->stack[types->use++]->name = "_apply";
		break;
	case LAMBDA: {
		Type *arg = lookup(types, ctx->locals, types->use, t->v);
		populate_definitions(ctx, types, ctx->locals, t->v); /* resolve */
		reverse(types, ctx->locals, types->use);
		simplify(ctx, types, ctx->locals);
		types->stack[ctx->locals] =
			Function(ctx, arg, types->stack[types->use-- - 1]);
		types->stack[ctx->locals++]->name = "";
		types->use = ctx->locals;
		break;
	}
	case LET:
	case LETREC:
		types->stack[ctx->locals - 1]->name = t->v; /* name let var */
		populate_definitions(ctx, types, ctx->locals, NULL);
		reverse(types, ctx->locals, types->use);
		simplify(ctx, types, ctx->locals);
		break;
	}
}

/* Gets the child pointer */
static Term *
get_child(Term *t, ssize_t offset)
{
	if(offset == -1)
		return NULL;
	return *(Term **)((uintptr_t)t + offset);
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
	TStack types = {{NULL}, 0, MAX_DEPTH};
	/* maps from type and seen to offset of child pointer */
	ssize_t child_offsets[5][3] = {
		[IDENTIFIER] = {-1},
		[APPLY] = {offsetof(Term, fn), offsetof(Term, arg), -1},
		[LAMBDA] = {offsetof(Term, body), -1},
		[LET] = {offsetof(Term, defn), offsetof(Term, body), -1},
		[LETREC] = {offsetof(Term, defn), offsetof(Term, body), -1},
	};
	while(env != NULL) {
		types.stack[types.use] = env->node;
		types.stack[types.use++]->name = env->name;
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
	ctx->result = types.stack[types.use - 1];
	return OK;
}
