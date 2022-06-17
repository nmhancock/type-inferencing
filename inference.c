#include <errno.h>
#include <stdlib.h>	/* strtol */
#include <string.h>	/* strcmp */
#include <stddef.h>	/* offsetof */

#include "inference.h"
#include <stdio.h> /* debug */

#define MAX_VARS 20 /* Max vars in same context, used in fresh & freshrec */
#define MAX_DEPTH 20

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
#include <assert.h> /* Debugging */
static int
occurs_in_type(Type *v, Type *type2)
{
	size_t cmp_use = 0;
	size_t cmp_cap = MAX_VARS;
	struct Type *cmp_types[MAX_VARS] = {0};
	size_t parent_use = 0;
	size_t parent_cap = MAX_VARS;
	struct Type *parent_types[MAX_VARS] = {0};
	Type *pruned_type2 = prune(type2);
	if(pruned_type2->type == OPERATOR) {
		parent_types[parent_use++] = pruned_type2;
		assert(parent_use < parent_cap);
	} else {
		cmp_types[cmp_use++] = pruned_type2;
		assert(cmp_use < cmp_cap);
	}
	while(parent_use > 0) {
		Type *cur = prune(parent_types[parent_use-- - 1]);
		for(int i = 0; i < cur->args; ++i) {
			if(cur->types[i]->type == OPERATOR) {
				parent_types[parent_use++] = cur->types[i];
				assert(parent_use < parent_cap);
			} else {
				cmp_types[cmp_use++] = cur->types[i];
				assert(cmp_use < cmp_cap);
			}
		}
		cmp_types[cmp_use++] = cur;
		assert(cmp_use < cmp_cap);
	}
	while(cmp_use > 0)
		if(prune(cmp_types[cmp_use-- - 1]) == v)
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
		for(int i = 0; i < ret->args; ++i)
			ret->types[i] = freshrec(ctx, p->types[i], ngs, map);
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

#define MAX_DEP MAX_DEPTH
static Type *
fresh_i(Inferencer *ctx, Type *t, TypeList *ngs)
{
	TypeMap map[MAX_DEP] = {{.next = NULL, .from = NULL, .to = NULL}};
	size_t cmp_use = 0;
	size_t cmp_cap = MAX_DEP;
	struct Type *cmp_types[MAX_DEP] = {0};
	struct Type **cmp_parents[MAX_DEP] = {0};
	size_t parent_use = 0;
	size_t parent_cap = MAX_DEP;
	struct Type *parent_types[MAX_DEP] = {0};
	size_t parent_args[MAX_DEP] = {0};
	size_t parent_cmp[MAX_DEP] = {0};
	if(ctx->error)
		return NULL;
	for(int i = 0; i < MAX_DEP - 1; ++i)
		map[i].next = &map[i + 1];
	Type *p = prune(t);
	if(p->type == OPERATOR) {
		parent_types[parent_use] = p;
		parent_cmp[parent_use++] = cmp_use;
		assert(parent_use < parent_cap);
	}
	cmp_types[cmp_use] = p;
	cmp_parents[cmp_use++] = NULL;
	while(parent_use > 0) {
		Type *cur = prune(parent_types[parent_use - 1]);
		size_t args = parent_args[parent_use - 1];
		parent_args[parent_use - 1] += 1;
		if(args == cur->args) { /* Stop processing this node */
			parent_use--;
			continue;
		}
		/* Look at child */
		if(cur->types[args]->type == OPERATOR) {
			parent_types[parent_use] = cur->types[args];
			parent_args[parent_use] = 0;
			parent_cmp[parent_use++] = cmp_use;
			assert(parent_use < parent_cap);
		}
		cmp_types[cmp_use] = cur->types[args];
		cmp_parents[cmp_use++] = &cur->types[args];
		assert(cmp_use < cmp_cap);
	}
	for(int i = 0; i < cmp_use; ++i) {
		Type *cur = prune(cmp_types[i]);
		Type **parent = cmp_parents[i];
		switch(cur->type) {
		case VARIABLE: {
			Type *p = cur;
			if(is_generic(p, ngs))
				p = find_or_add(ctx, map, p);
			break;
		}
		case OPERATOR: {
			Type *p = make_type(ctx);
			*p = (Type){.type = OPERATOR,
				    .name = cur->name,
				    .args = 0};
			break;
		}
		}
		if(parent)
			*parent = p;
	}
	return NULL;
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
extern_analyze2(Inferencer *ctx, Term *node, Env *env, TypeList *ngs)
{
	ctx->result = analyze(ctx, node, env, ngs);
	if(ctx->error)
		return ctx->error;
	else
		return OK;
}

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
typush(Type **stack, size_t tyuse, Type* v)
{
	stack[tyuse++] = v;
	return tyuse;
}

#include <stdio.h>
static error_t
unify_i(Type *t1, Type *t2)
{
	typedef struct {
		Type *a;
		Type *b;
	} Pair;
	Pair stack[MAX_DEPTH] = {{ NULL, NULL }};
	size_t suse = 0;
	size_t scap = MAX_DEPTH;
	error_t res = OK;
	stack[suse++] = (Pair) {.a = t1, .b = t2};
	while(suse > 0) {
		Type *a = prune(stack[suse - 1].a);
		Type *b = prune(stack[suse-- - 1].b);
		if(a->type == OPERATOR && b->type == VARIABLE) { /* Normalize */
			Type *swp = a;
			a = b;
			b = swp;
		}
		switch(a->type) {
		case VARIABLE:
			if(a != b && occurs_in_type(a, b))
				return RECURSIVE_UNIFICATION;
			else
				a->instance = b;
			break;
		case OPERATOR:
			if(a == b)
				break;
			else if(strcmp(a->name, b->name) || a->args != b->args)
				return TYPE_MISMATCH;
			else
				for(int i = 0; i < a->args; ++i)
					stack[suse++] = (Pair){a->types[i], b->types[i]};
			break;
		}
	}
	return OK;
}

static Type*
lookup(Env *env, char* name)
{
	Env *cur = env;
	while(cur != NULL && strcmp(name, cur->name))
		cur = cur->next;
	return cur ? cur->node : NULL;
}

#define MAX_TYPES 200
error_t
extern_analyze(Inferencer *ctx, Term *node, Env *env, TypeList *ngs)
{
	TypeMap map[MAX_DEP] = {{.next = NULL, .from = NULL, .to = NULL}};
	Type *stack[MAX_DEP] = {NULL};
	Term *ordered[MAX_TYPES] = {NULL};
	size_t scap = MAX_DEP;
	size_t suse = 0;
	size_t ocap = MAX_TYPES;
	size_t ouse = left_depth_first(ordered, ocap, node);
	if(ouse == (size_t)-1)
		return OUT_OF_TYPES;
	else if(ouse == (size_t)-2)
		return MAX_RECURSION_EXCEEDED;
	for(size_t i = 0; i < MAX_DEP - 1; ++i)
		map[i].next = &map[i + 1];
	map[MAX_DEP - 1].next = NULL;
	for(size_t i = 0; i < ouse; ++i) {
		Term *t = ordered[i];
		if(ctx->error)
			return ctx->error;
		fprintf(stderr, "i: %zu t->type: %d\n", i, t->type);
		switch(t->type) {
		case IDENTIFIER: {
			long l = strtol(t->name, NULL, 0);
			Type *cur = lookup(env, t->name);
			if(lookup(env, t->name)) {
				suse = typush(stack, suse, cur);
				fprintf(stderr, "pushing %s as %d %p\n", t->name, cur->type, cur);
			}
			else if(l != 0 || (l == 0 && errno != EINVAL)) {
				suse = typush(stack, suse, Integer(ctx));
				fprintf(stderr, "pushing %s as int %p\n", t->name, Integer(ctx));
			}
			else {
				Type *v = Var(ctx);
				suse = typush(stack, suse, v);
				fprintf(stderr, "pushing %s as var %p\n", t->name, v);
			}
			break;
		}
		case APPLY: {
			Type *arg = prune(stack[suse-- - 1]);
			Type *fn = stack[suse-- - 1];
			fprintf(stderr, "arg type: %d %p\n", arg->type, arg);
			fprintf(stderr, "fn type: %d %p\n", fn->type, fn);
			Type *var = Var(ctx);
			fprintf(stderr, "fn args: %d\n", fn->args);
			error_t res = unify_i(Function(ctx, arg, var), fn);
			if(res) {
				fprintf(stderr, "Error: %d\n", res);
				fprintf(stderr, "test: %p %p\n", prune(arg), prune(fn));
				return res;
			}
			suse = typush(stack, suse, var);
			break;
		}
		case LAMBDA:
			fprintf(stderr, "suse: %zu\n", suse);
			for(size_t j = suse; j > 0; j--)
				fprintf(stderr, "type: %d\n", stack[suse - 1]->type);
			assert(0);	
			break;
		case LET:
			break;
		case LETREC:
			break;
		}
	}
	return OK;
}
