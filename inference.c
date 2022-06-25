#include <errno.h>
#include <stdlib.h>	/* strtol */
#include <string.h>	/* strcmp */
#include <stddef.h>	/* offsetof */
#include <stdbool.h>	/* true, false */

#include "inference.h"

#define MAX_DEPTH 30

typedef struct TypeList {
	Type *val;
	struct TypeList *next;
} TypeList;

size_t
tpush(Term **list, size_t tuse, size_t tcap, Term *t)
{
	list[tuse++] = t;
	return tuse;
}

size_t
spush(size_t *list, size_t luse, size_t lcap, size_t v)
{
	list[luse++] = v;
	return luse;
}

/* Gets the child pointer */
Term *
get_child(Term *t, ssize_t offset)
{
	if(offset == -1)
		return NULL;
	return *(Term **)((uintptr_t)t + offset);
}

size_t
left_depth_first(Term **terms, size_t tcap, Term *root)
{
	Term *inproc[MAX_DEPTH] = {NULL}; /* stack for parent nodes */
	size_t pseen[MAX_DEPTH] = {0}; /* num times has the parent been seen */
	size_t puse = 0, pcap = MAX_DEPTH;
	/* maps from type and seen to offset of child pointer */
	ssize_t child_offsets[5][3] = {
		[IDENTIFIER] = {-1},
		[APPLY] = {offsetof(Term, fn), offsetof(Term, arg), -1},
		[LAMBDA] = {offsetof(Term, body), -1},
		[LET] = {offsetof(Term, defn), offsetof(Term, body), -1},
		[LETREC] = {offsetof(Term, defn), offsetof(Term, body), -1},
	};
	size_t tuse = 0;
	if(!root)
		return tuse;
	puse = tpush(inproc, puse, pcap, root);
	while(puse > 0) {
		Term *term = inproc[puse - 1];
		size_t seen = pseen[puse-- - 1];
		Term *child = get_child(term, child_offsets[term->type][seen]);
		if(!child) {
			tuse = tpush(terms, tuse, tcap, term);
			if (tuse == tcap)
				return (size_t)-1;
		} else {
			(void)tpush(inproc, puse, pcap, term);
			puse = spush(pseen, puse, pcap, seen + 1);
			if (puse == pcap)
				return (size_t)-2;
			(void)tpush(inproc, puse, pcap, child);
			puse = spush(pseen, puse, pcap, 0);
			if (puse == pcap)
				return (size_t)-2;
		}
	}
	return tuse;
}

static size_t
typush(Type **stack, size_t suse, Type* v)
{
	stack[suse++] = v;
	return suse;
}

static size_t
cpush(char **stack, size_t suse, char *v)
{
	stack[suse++] = v;
	return suse;
}

static
size_t push_env(Type **stack, char **nstack, size_t scap, Env *env)
{
	size_t suse = 0;
	for(Env *cur = env; cur != NULL && suse < scap; cur = cur->next) {
		(void)typush(stack, suse, cur->node);
		suse = cpush(nstack, suse, cur->name);
	}
	return suse;
}

static Type*
lookup(Type **types, char **names, size_t cap, char *name)
{
	for(size_t i = 0; i < cap; ++i)
		if(!strcmp(name, names[i]))
			return types[i];
	return NULL;
}

static
error_t analyze_wip(Inferencer *ctx, Term **ordered, size_t ouse, Env *env)
{
	Type *stack[MAX_DEPTH] = {NULL};
	char *nstack[MAX_DEPTH] = {NULL};
	size_t locals = 0;
	size_t scap = MAX_DEPTH;
	size_t suse = push_env(stack, nstack, scap, env);
	locals = suse;
	if(ouse == (size_t)-1)
		return OUT_OF_TYPES;
	else if(ouse == (size_t)-2)
		return MAX_RECURSION_EXCEEDED;
	Type *apply = Function(ctx, NULL, NULL);
	apply->args = 0;
	apply->name = "_apply";
	for(size_t i = 0; i < ouse; ++i) {
		Term *t = ordered[i];
		if(ctx->error)
			return ctx->error;
		switch(t->type) {
		case IDENTIFIER: {
			/* Lookup name in local scope _only_ since we don't
			 * know which, if any, global variables are shadowed
			*/
			Type *v = lookup(&stack[locals],
					 &nstack[locals],
					 suse - locals,
					 t->name);
			long l = strtol(t->name, NULL, 0);
			if(l != 0 || (l == 0 && errno != EINVAL))
				v = Integer(ctx);
			else if (!v) {
				v = Var(ctx);
				v->name = t->name;
			}
			(void)typush(stack, suse, v);
			suse = cpush(nstack, suse, t->name);
			break;
		}
		case APPLY: {
			(void)typush(stack, suse, apply);
			suse = cpush(nstack, suse, "_apply");
			break;
		}
		case LAMBDA: {
			Type *arg = NULL;
			for(size_t i = locals; i < suse; ++i) { /* resolve */
				Type *found = lookup(stack, nstack, locals, stack[i]->name);
				if(found && strcmp(stack[i]->name, t->v))
					stack[i] = found;
			}
			for(size_t i = locals; i < suse; ++i) {
				if(!strcmp(stack[i]->name, t->v))
					arg = stack[i];
			}
			for(size_t low = locals, high = suse - 1; low < high; low++, high--) { /* reverse */
				Type *tmp = stack[low];
				char *ctmp = nstack[low];
				stack[low] = stack[high];
				nstack[low] = nstack[high];
				stack[high] = tmp;
				nstack[high] = ctmp;
			}
			for(size_t i = locals, applies = 0, end = suse; i < end; i++) { /* simplify */
				Type *cur = stack[i];
				if(cur == apply) {
					applies++;
					continue;
				}
				if(cur->type == VARIABLE && cur->instance)
					cur = cur->instance;
				while(suse - end > 0 && applies > 0 && cur->type == OPERATOR && cur->args > 0) {
					Type *prev = stack[suse-- - 1];
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
				suse = typush(stack, suse, cur);
			}
			stack[locals] = Function(ctx, arg, stack[suse-- - 1]);
			locals = cpush(nstack, locals, NULL); // Don't know name yet
			suse = locals;
			break;
		}
		case LET:
		case LETREC:
			nstack[locals - 1] = t->v; /* name variable from let */
			for(size_t i = locals; i < suse; ++i) { /* resolve */
				Type *found = lookup(stack, nstack, locals, stack[i]->name);
				if(found)
					stack[i] = found;
			}
			for(size_t low = locals, high = suse - 1; low < high; low++, high--) { /* swap */
				Type *tmp = stack[low];
				char *ctmp = nstack[low];
				stack[low] = stack[high];
				nstack[low] = nstack[high];
				stack[high] = tmp;
				nstack[high] = ctmp;
			}
			for(size_t i = locals, applies = 0, end = suse; i < end; i++) { /* simplify */
				Type *cur = stack[i];
				if(cur == apply) {
					applies++;
					continue;
				}
				if(cur->type == VARIABLE && cur->instance)
					cur = cur->instance;
				while(suse - end > 0 && applies > 0 && cur->type == OPERATOR && cur->args > 0) {
					Type *prev = stack[suse-- - 1];
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
				suse = typush(stack, suse, cur);
			}
			break;
		}
	}
	ctx->result = stack[suse - 1];
	return OK;
}

#define MAX_TYPES 200
error_t
extern_analyze(Inferencer *ctx, Term *node, Env *env, TypeList *ngs)
{
	Term *ordered[MAX_TYPES] = {NULL};
	size_t ocap = MAX_TYPES;
	size_t ouse = left_depth_first(ordered, ocap, node);
	(void)ngs;
	return analyze_wip(ctx, ordered, ouse, env);
}
