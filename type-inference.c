#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define MAX_ARGS 2
#define MAX_VARS 10

#define MAX_TYPES 200
#define MAX_MP_ITEM 20 /* Used in fresh & freshrec */

enum ast_node_type {
  IDENTIFIER = 0,
  APPLY = 1,
  LAMBDA = 2,
  LET = 3,
  LETREC = 4
};

struct ast_node {
  union {
    struct {
      char* name;
    };
    struct {
      struct ast_node* fn;
      struct ast_node* arg;
    };
    struct {
      char* v;
      struct ast_node* defn;
      struct ast_node* body;
    };
  };
  enum ast_node_type type;
};
void print_ast(struct ast_node* n);

enum lang_type_type {
  VARIABLE = 0,
  FUNCTION = 1,
  OPERATOR = 2,
  UNHANDLED_SYNTAX_NODE = -1,
  UNDEFINED_SYMBOL = -2,
  RECURSIVE_UNIFICATION = -3,
  TYPE_MISMATCH = -4,
  UNIFY_ERROR = -5,
};

struct lang_type {
  union {
    struct {
      struct lang_type* instance;
      char* var_name;
      int id;
    };
    struct {
      struct lang_type* from_type;
      struct lang_type* to_type;
    };
    struct {
      char* op_name;
      int args;
      struct _lt_item {
        struct lang_type *val;
        struct _lt_item* next;
      } types[MAX_ARGS];
    };
    char* undefined_symbol;
  };
  enum lang_type_type type;
};
void print_type(struct lang_type* t);

/* Global context to keep function signatures the same */
struct lang_type types[MAX_TYPES];
struct lang_type* Integer = &types[0];
struct lang_type* Bool = &types[1];
struct inferencing_ctx { struct lang_type* types; int current_type; };
struct inferencing_ctx ctx = { .types = &types, .current_type = 2 };

struct env {
  char* name;
  struct lang_type* node;
  struct env* next;
};

struct lang_type* make_type(int* idx)
{
  assert(ctx.current_type < MAX_TYPES);
  if (idx)
    *idx = ctx.current_type;
  return &ctx.types[ctx.current_type++];
}

struct lang_type* Var()
{
  int id;
  struct lang_type* result_type = make_type(&id);
  *result_type = (struct lang_type) {
    .type = VARIABLE,
    .instance = NULL,
    .var_name = NULL,
    .id = id
  };
  return result_type;
}

static char* fname = "->";
struct lang_type* Function(struct lang_type* arg_t, struct lang_type* res_t)
{
  struct lang_type* function = make_type(NULL);
  /* Unrolling the constructor to keep the same form as the old code. */
  *function = (struct lang_type) {
    .type = OPERATOR,
    .op_name = fname,
    .args = 2,
    .types = { 
      { .val = arg_t, .next = &function->types[1] },
      { .val = res_t, .next = NULL }
    }
  };
  return function;
}

struct lang_type* prune(struct lang_type* t)
{
  /*
   * Returns the currently defining instance of t.
   
   * As a side effect, collapses the list of type instances. The function Prune
   * is used whenever a type expression has to be inspected: it will always
   * return a type expression which is either an uninstantiated type variable or
   * a type operator; i.e. it will skip instantiated variables, and will
   * actually prune them from expressions to remove long chains of instantiated
   * variables.
  */
  switch (t->type) {
  case VARIABLE:
    if (t->instance == NULL)
      return t;
    t->instance = prune(t->instance);
    return t->instance;
  default:
    return t;
  }
}

int occurs_in_type(struct lang_type* v, struct lang_type* type2)
{
  struct lang_type* pruned_type2 = prune(type2);
  if (pruned_type2 == v)
    return 1;
  if (pruned_type2->type == OPERATOR) {
    struct _lt_item* cmp = pruned_type2->types;
    while (cmp != NULL && cmp->val != NULL) {
      if (occurs_in_type(v, cmp->val))
        return 1;
      cmp = cmp->next;
    }
  }
  return 0;
}

int is_generic(struct lang_type* v, struct _lt_item* non_generic)
{
  /* Flip the return value because we're checking non_generics for a generic */
  while (non_generic != NULL) {
    if (occurs_in_type(v, non_generic->val))
      return 0;
    non_generic = non_generic->next;
  }
  return 1;
}

struct _mp_item {
  struct lang_type* from;
  struct lang_type* to;
  struct _mp_item* next;
};

struct lang_type* freshrec(struct lang_type* tp, struct _lt_item* non_generic, struct _mp_item* map)
{
  struct lang_type* p = prune(tp);
  switch (p->type) {
  case VARIABLE: {
    if (!is_generic(p, non_generic))
      return p;
    struct _mp_item *cur = map, *prev = cur;
    while (cur->from != NULL) {
      assert(cur != NULL); /* FIXME, prealloc */
      if (cur->from == p) {
        return cur->to;
      }
      prev = cur;
      cur = cur->next;
    }
    cur->from = p;
    cur->to = Var();
    return cur->to;
  }
  case OPERATOR: {
    struct lang_type* ret = make_type(NULL);
    *ret = (struct lang_type) {
      .type = OPERATOR, .op_name = p->op_name, .args = p->args
    };
    for (int i = 0; i < MAX_ARGS - 1; ++i) {
      ret->types[i].next = &ret->types[i+1];
    }
    ret->types[MAX_ARGS - 1].next = NULL;
    for (int i = 0; i < ret->args; ++i) {
      ret->types[i].val = freshrec(p->types[i].val, non_generic, map);
    }
    return ret;
  }
  default: {
    struct lang_type* ret = make_type(NULL);
    *ret = (struct lang_type) { .type = UNIFY_ERROR };
    return ret;
  }
  }
};

struct lang_type* fresh(struct lang_type* t, struct _lt_item* non_generic)
{
  struct _mp_item map[MAX_MP_ITEM];
  for (int i = 0; i < MAX_MP_ITEM - 1; ++i) {
    map[i].next = &map[i+1];
  }
  for (int i = 0; i < MAX_MP_ITEM; ++i) {
    map[i].from = map[i].to = NULL;
  }
  map[MAX_MP_ITEM - 1].next = NULL;
  return freshrec(t, non_generic, map);
}

struct lang_type* get_type(char* name, struct env* env, struct _lt_item* non_generic)
{
  long l = strtol(name, NULL, 0);
  struct lang_type* res = make_type(NULL);
  *res = (struct lang_type) { .type = UNDEFINED_SYMBOL, .undefined_symbol = name };
  if (l != 0 || l == 0 && errno != EINVAL) {
    return Integer;
  }
  struct env* cur = env;
  while (cur != NULL) {
    if (!strcmp(name, cur->name))
      return fresh(cur->node, non_generic);
    cur = cur->next;
  }
  return res;
}

struct lang_type* unify(struct lang_type* t1, struct lang_type* t2)
{
  struct lang_type* a = prune(t1);
  struct lang_type* b = prune(t2);

  switch(a->type) {
  case VARIABLE:
    if (a == b)
      return a; /* Arbitrary, they're already equal! */
    if (occurs_in_type(a, b)) {
      struct lang_type* ret = make_type(NULL);
      *ret = (struct lang_type) { .type = RECURSIVE_UNIFICATION };
      return ret;
    }
    a->instance = b;
    return a;
  case OPERATOR:
    if (b->type == VARIABLE)
      return unify(b, a);
    if (b->type == OPERATOR) {
      if (strcmp(a->op_name, b->op_name) || a->args != b->args) {
        struct lang_type* ret = make_type(NULL);
        *ret = (struct lang_type) { .type = TYPE_MISMATCH };
        return ret;
      }
      struct _lt_item *at = a->types;
      struct _lt_item *bt = b->types;
      while (at != NULL && at->val != NULL && bt != NULL && bt->val != NULL) {
        (void)unify(at->val, bt->val);
        at = at->next;
        bt = bt->next;
      }
      return a;
    }
  default:
    if (a->type < 0)
      return a;
    if (b->type < 0)
      return b;
    struct lang_type* ret = make_type(NULL);
    *ret = (struct lang_type) { .type = UNIFY_ERROR };
    return ret;
  }
}

struct lang_type* analyze(struct ast_node* node, struct env* env, struct _lt_item* non_generic)
{
  switch(node->type) {
  case IDENTIFIER:
    return get_type(node->name, env, non_generic);
  case APPLY: {
    struct lang_type* fun_type = analyze(node->fn, env, non_generic);
    struct lang_type* arg_type = analyze(node->arg, env, non_generic);
    struct lang_type* result_type = Var();
    if (fun_type->type < 0)
      return fun_type;
    if (arg_type->type < 0)
      return arg_type;
    (void) unify(Function(arg_type, result_type), fun_type);
    return result_type;
  }
  case LAMBDA: {
    struct lang_type* arg_type = Var();
    struct env new_env = {
      .name = node->v,
      .node = arg_type,
      .next = env,
    };
    struct _lt_item new_non_generic = { .val = arg_type, .next = non_generic };
    struct lang_type* result_type = analyze(node->body, &new_env, &new_non_generic);
    if (result_type->type < 0)
      return result_type;
    return Function(arg_type, result_type);
  }
  case LET: {
    struct lang_type* defn_type = analyze(node->defn, env, non_generic);
    if (defn_type->type < 0)
      return defn_type;
    struct env new_env = {
      .name = node->v,
      .node = defn_type,
      .next = env,
    };
    return analyze(node->body, &new_env, non_generic);
  }
  case LETREC: {
    struct lang_type* new_type = Var();
    struct env new_env = {
      .name = node->v,
      .node = new_type,
      .next = env,
    };
    struct _lt_item new_non_generic = { .val = new_type, .next = non_generic };
    struct lang_type* defn_type = analyze(node->defn, &new_env, &new_non_generic);
    if (defn_type->type < 0)
      return defn_type;
    (void) unify(new_type, defn_type);
    return analyze(node->body, &new_env, non_generic);
  }
  default: {
    struct lang_type* res = make_type(NULL);
    *res = (struct lang_type) { .type = UNHANDLED_SYNTAX_NODE };
    return res;
  }
  }
}

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
  struct lang_type* pair_type = make_type(NULL);
  *pair_type = (struct lang_type) {
    .type = OPERATOR,
    .op_name = "*",
    .args = 2,
    .types = { 
      { .val = var1, .next = &pair_type->types[1] },
      { .val = var2, .next = NULL }
    }
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

  struct lang_type* t = analyze(&factorial, my_env, NULL);

  print(&factorial, t);
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
      char* type0 = print_a_type(t->types[0].val);
      char* type1 = print_a_type(t->types[1].val);
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
