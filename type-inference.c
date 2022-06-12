#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "type-inference.h"

#define MAX_VARS 10
#define MAX_TYPES 200
#define MAX_MP_ITEM 20 /* Used in fresh & freshrec */

/* Global context to keep function signatures the same */
struct lang_type types[MAX_TYPES];
struct lang_type* Integer = &types[0];
struct lang_type* Bool = &types[1];
struct inferencing_ctx { struct lang_type* types; int current_type; };
struct inferencing_ctx ctx = { .types = &types, .current_type = 2 };

/* Debugging functions */
static void print_ast(struct ast_node* n);
static void print_type(struct lang_type* t);

struct lang_type* make_type(void)
{
  assert(ctx.current_type < MAX_TYPES);
  ctx.types[ctx.current_type].id = ctx.current_type;
  return &ctx.types[ctx.current_type++];
}

struct lang_type* Var()
{
  struct lang_type* result_type = make_type();
  *result_type = (struct lang_type) {
    .type = VARIABLE,
    .instance = NULL,
    .var_name = NULL,
  };
  return result_type;
}

static char* fname = "->";
struct lang_type* Function(struct lang_type* arg_t, struct lang_type* res_t)
{
  struct lang_type* function = make_type();
  /* Unrolling the constructor to keep the same form as the old code. */
  *function = (struct lang_type) {
    .type = OPERATOR,
    .op_name = fname,
    .args = 2,
    .types = { arg_t, res_t },
  };
  return function;
}

struct lang_type* Err(enum lang_type_type err, char* symbol)
{
  struct lang_type* error = make_type();
  *error = (struct lang_type) { .type = err, .undefined_symbol = symbol };
  return error;
}

struct lang_type* prune(struct lang_type* t)
{
  if (t->type != VARIABLE)
    return t;
  if (t->instance == NULL)
    return t;
  t->instance = prune(t->instance);
  return t->instance;
}

int occurs_in_type(struct lang_type* v, struct lang_type* type2)
{
  struct lang_type* pruned_type2 = prune(type2);
  if (pruned_type2 == v)
    return 1;
  if (pruned_type2->type == OPERATOR)
    for (int i = 0; i < pruned_type2->args; ++i)
      if (occurs_in_type(v, pruned_type2->types[i]))
        return 1;
  return 0;
}

struct lt_list {
  struct lang_type* val;
  struct lt_list* next;
};
int is_generic(struct lang_type* v, struct lt_list* ngs)
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
  struct lang_type* from;
  struct lang_type* to;
  struct _mp_item* next;
};

struct lang_type* freshrec(
  struct lang_type* tp, struct lt_list* ngs, struct _mp_item* map
)
{
  struct lang_type* p = prune(tp);
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
      return Err(LOCAL_SCOPE_EXCEEDED, NULL);
    cur->from = p;
    cur->to = Var();
    return cur->to;
  }
  case OPERATOR: {
    struct lang_type* ret = make_type();
    *ret = (struct lang_type) {
      .type = OPERATOR, .op_name = p->op_name, .args = p->args
    };
    for (int i = 0; i < ret->args; ++i) {
      ret->types[i] = freshrec(p->types[i], ngs, map);
    }
    return ret;
  }
  default:
    return Err(UNIFY_ERROR, NULL);
  }
};

struct lang_type* fresh(struct lang_type* t, struct lt_list* ngs)
{
  struct _mp_item map[MAX_MP_ITEM];
  for (int i = 0; i < MAX_MP_ITEM - 1; ++i) {
    map[i].next = &map[i+1];
  }
  map[MAX_MP_ITEM - 1].next = NULL;
  for (int i = 0; i < MAX_MP_ITEM; ++i) {
    map[i].from = map[i].to = NULL;
  }
  return freshrec(t, ngs, map);
}

struct env {
  char* name;
  struct lang_type* node;
  struct env* next;
};
struct lang_type* get_type(char* name, struct env* env, struct lt_list* ngs)
{
  long l = strtol(name, NULL, 0);
  if (l != 0 || l == 0 && errno != EINVAL) {
    return Integer;
  }
  struct env* cur = env;
  while (cur != NULL) {
    if (!strcmp(name, cur->name))
      return fresh(cur->node, ngs);
    cur = cur->next;
  }
  return Err(UNDEFINED_SYMBOL, name);
}

struct lang_type* unify(struct lang_type* t1, struct lang_type* t2)
{
  struct lang_type* a = prune(t1);
  struct lang_type* b = prune(t2);

  switch(a->type) {
  case VARIABLE:
    if (a == b)
      return a; /* Arbitrary, they're already equal! */
    if (occurs_in_type(a, b))
      return Err(RECURSIVE_UNIFICATION, NULL);
    a->instance = b;
    return a;
  case OPERATOR:
    if (b->type == VARIABLE)
      return unify(b, a);
    if (b->type == OPERATOR) {
      if (strcmp(a->op_name, b->op_name) || a->args != b->args)
        Err(TYPE_MISMATCH, NULL);
      for (int i = 0; i < a->args; ++i)
        (void)unify(a->types[i], b->types[i]);
      return a;
    }
  default:
    if (a->type < 0)
      return a;
    if (b->type < 0)
      return b;
    return Err(UNIFY_ERROR, NULL);
  }
}

struct lang_type* analyze(
    struct ast_node* node, struct env* env, struct lt_list* ngs
)
{
  switch(node->type) {
  case IDENTIFIER:
    return get_type(node->name, env, ngs);
  case APPLY: {
    struct lang_type* fun_type = analyze(node->fn, env, ngs);
    if (fun_type->type < 0)
      return fun_type;
    struct lang_type* arg_type = analyze(node->arg, env, ngs);
    if (arg_type->type < 0)
      return arg_type;
    struct lang_type* result_type = Var();
    (void) unify(Function(arg_type, result_type), fun_type);
    return result_type;
  }
  case LAMBDA: {
    struct lang_type* arg_type = Var();
    struct env new_env = { .name = node->v, .node = arg_type, .next = env };
    struct lt_list new_ng = { .val = arg_type, .next = ngs };
    struct lang_type* result_type = analyze(node->body, &new_env, &new_ng);
    if (result_type->type < 0)
      return result_type;
    return Function(arg_type, result_type);
  }
  case LET: {
    struct lang_type* defn_type = analyze(node->defn, env, ngs);
    if (defn_type->type < 0)
      return defn_type;
    struct env new_env = { .name = node->v, .node = defn_type, .next = env };
    return analyze(node->body, &new_env, ngs);
  }
  case LETREC: {
    struct lang_type* new_type = Var();
    struct env new_env = { .name = node->v, .node = new_type, .next = env };
    struct lt_list new_ng = { .val = new_type, .next = ngs };
    struct lang_type* defn_type = analyze(node->defn, &new_env, &new_ng);
    if (defn_type->type < 0)
      return defn_type;
    (void) unify(new_type, defn_type);
    return analyze(node->body, &new_env, ngs);
  }
  default:
    return Err(UNHANDLED_SYNTAX_NODE, NULL);
  }
}

/* Usage examples */
#include <stdio.h>
#include <time.h>

void print(struct ast_node* n, struct lang_type* t);

int main(void)
{
  /* Basic types are constructed with a nullary type constructor */
  *Integer = (struct lang_type) {
    .type = OPERATOR, .op_name = "int", .types = NULL, .args = 0
  };
  *Bool = (struct lang_type) {
    .type = OPERATOR, .op_name = "bool", .types = NULL, .args = 0
  };

  struct lang_type* var1 = Var();
  struct lang_type* var2 = Var();
  struct lang_type* pair_type = make_type();
  *pair_type = (struct lang_type) {
    .type = OPERATOR,
    .op_name = "*",
    .args = 2,
    .types = { var1, var2 }
  };
  struct lang_type* var3 = Var();

  struct env envs[7] = {
    {
      .name = "pair",
      .node = Function(var1, Function(var2, pair_type)),
      .next = &envs[1]
    },
    { .name = "true", .node = Bool, .next = &envs[2] },
    {
      .name = "cond",
      .node = Function(Bool, Function(var3, Function(var3, var3))),
      .next = &envs[3],
    },
    { .name = "zero", .node = Function(Integer, Bool), .next = &envs[4] },
    { .name = "pred", .node = Function(Integer, Integer), .next = &envs[5] },
    {
      .name = "times",
      .node = Function(Integer, Function(Integer, Integer)),
      .next = &envs[6]
    },
    { .name = "factorial", .node = Function(Integer, Integer), .next = NULL }
  };
  struct env* my_env = &envs;

  struct ast_node factorial = {
    .type = LETREC,
    .v = "factorial",
    .defn = &(struct ast_node) {
      .type = LAMBDA,
      .v = "n",
      .body = &(struct ast_node) {
        .type = APPLY,
        .fn = &(struct ast_node) {
          .type = APPLY,
          .fn = &(struct ast_node) {
            .type = APPLY,
            .fn = &(struct ast_node) { .type = IDENTIFIER, .name = "cond" },
            .arg = &(struct ast_node) {
              .type = APPLY,
              .fn = &(struct ast_node) { .type = IDENTIFIER, .name = "zero" },
              .arg = &(struct ast_node) { .type = IDENTIFIER, .name = "n" }
            }
          },
          .arg = &(struct ast_node) { .type = IDENTIFIER, .name = "1" }
        },
        .arg = &(struct ast_node) {
          .type = APPLY,
          .fn = &(struct ast_node) {
            .type = APPLY,
            .fn = &(struct ast_node) { .type = IDENTIFIER, .name = "times" },
            .arg = &(struct ast_node) { .type = IDENTIFIER, .name = "n" },
          },
          .arg = &(struct ast_node) {
            .type = APPLY,
            .fn = &(struct ast_node) { .type = IDENTIFIER, .name = "factorial" },
            .arg = &(struct ast_node) {
              .type = APPLY,
              .fn = &(struct ast_node) { .type = IDENTIFIER, .name = "pred" },
              .arg = &(struct ast_node) { .type = IDENTIFIER, .name = "n" },
            }
          }
        }
      },
    },
    .body = &(struct ast_node) {
      .type = APPLY,
      .fn = &(struct ast_node) { .type = IDENTIFIER, .name = "factorial" },
      .arg = &(struct ast_node) { .type = IDENTIFIER, .name = "5" },
    }
  };

  struct lang_type* t;
  clock_t total = 0;
#define ITERATIONS 1000000
  for (int i = 0; i < ITERATIONS; ++i) {
    ctx.current_type = 16; /* Experimentally determined */
    clock_t tic = clock();
    t = analyze(&factorial, my_env, NULL);
    clock_t toc = clock();
    total += toc - tic;
  }
  print(&factorial, t);
  fprintf(stdout, "Iterations: %d Total time: %f ns\n",
      ITERATIONS, (double) (total / CLOCKS_PER_SEC * 1000000));
  return 0;
}

char* print_a_type(struct lang_type* t)
{
  char* ret;
  if (!t) {
    asprintf(&ret, "%s\n", "NULL");
    return ret;
  }
  switch(t->type) {
  case VARIABLE:
    if (!t->instance) {
      asprintf(&ret, "%s (%d)", t->var_name, t->id);
      return ret;
    }
    char* instance = print_a_type(t->instance);
    if (instance)
      return instance;
    asprintf(&ret, "NULL");
    return ret;
  case OPERATOR:
    if (t->args == 0)
      asprintf(&ret, "%s", t->op_name); /* Ensure caller can free as expected */
    else if (t->args == 2) {
      char* type0 = print_a_type(t->types[0]);
      char* type1 = print_a_type(t->types[1]);
      asprintf(
        &ret, "(%s %s %s)",
        type0 ? type0 : "NULL",
        t->op_name,
        type1 ? type1 : "NULL"
      );
      free(type0);
      free(type1);
    } else { /* TODO: Implement properly, don't be lazy! */
      asprintf(&ret, "%s", t->op_name); /* Ensure caller can free as expected */
    }
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

void print_type(struct lang_type* t)
{
  char* res = print_a_type(t);
  if (!res)
    printf("NULL\n");
  else
    printf("%s\n", res);
  free(res);
}

char* print_ast_node(struct ast_node* n)
{
  char* ret;
  if (!n) {
    asprintf(&ret, "%s\n", "NULL");
    return ret;
  }
  switch(n->type) {
  case IDENTIFIER: {
    asprintf(&ret, "%s", n->name ? n->name : "NULL");
    return ret;
  }
  case APPLY: {
    char* fn = print_ast_node(n->fn);
    char* arg = print_ast_node(n->arg);
    asprintf(&ret, "(%s %s)", fn ? fn : "NULL", arg ? arg : "NULL");
    free(fn);
    free(arg);
    return ret;
  }
  case LAMBDA: {
    char* body = print_ast_node(n->body);
    asprintf(&ret, "(fn %s => %s)", n->v, body ? body : "NULL");
    free(body);
    return ret;
  }
  case LET: {
    char* body = print_ast_node(n->body);
    char* defn = print_ast_node(n->defn);
    asprintf(
      &ret,
      "(let %s = %s in %s)",
      n->v,
      defn ? defn : "NULL",
      body ? body : "NULL");
    free(defn);
    free(body);
    return ret;
  }
  case LETREC: {
    char* body = print_ast_node(n->body);
    char* defn = print_ast_node(n->defn);
    asprintf(
      &ret,
      "(letrec %s = %s in %s)",
      n->v,
      defn ? defn : "NULL",
      body ? body : "NULL");
    free(defn);
    free(body);
    return ret;
  }
  }
  return ret;
}

void print_ast(struct ast_node* n)
{
  char* res = print_ast_node(n);
  if (!res)
    printf("NULL\n");
  else
    printf("%s\n", res);
  free(res);
}

void print(struct ast_node* n, struct lang_type* t)
{
  char* ast = print_ast_node(n);
  char* type = print_a_type(t);
  printf("%s : %s\n", ast, type);
  free(type);
  free(ast);
}
