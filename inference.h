#define MAX_ARGS 2

typedef enum {
	OK = 0,
	UNHANDLED_SYNTAX_NODE = -1,
	UNDEFINED_SYMBOL = -2,
	RECURSIVE_UNIFICATION = -3,
	TYPE_MISMATCH = -4,
	UNIFY_ERROR = -5,
	LOCAL_SCOPE_EXCEEDED = -6,
	OUT_OF_TYPES = -7,
} error_t;
typedef enum {
	VARIABLE = 0,
	OPERATOR = 1,
} type_t;
typedef struct Type {
	union {
		struct Type *instance;
		struct {
			struct Type *types[MAX_ARGS];
			int args;
		};
	};
	char *name;
	int id;
	type_t type;
} Type;
typedef enum {
	IDENTIFIER = 0,
	APPLY = 1,
	LAMBDA = 2,
	LET = 3,
	LETREC = 4
} term_t;
typedef struct Term {
	union {
		struct {
			char *name;
		};
		struct {
			struct Term *fn;
			struct Term *arg;
		};
		struct {
			char *v;
			struct Term *defn;
			struct Term *body;
		};
	};
	term_t type;
} Term;

typedef struct TypeList TypeList;
typedef struct Env {
	char *name;
	Type *node;
	struct Env *next;
} Env;
typedef struct Inferencer {
	Type *types;
	Type *result;
	char *error_msg;
	error_t error;
	int use;
	int cap;
} Inferencer;
error_t extern_analyze(Inferencer *ctx, Term *node, Env *env, TypeList *ngs);
Type *get_result(Inferencer *ctx);

Inferencer make_ctx(Type *, int); /* TODO: Fix return type to be int */
Type *make_type(Inferencer *);
Type *Function(Inferencer *, Type *, Type *);
Type *Var(Inferencer *);
Type *Err(Inferencer *, type_t, char *);
Type *Integer(Inferencer *);
Type *Bool(Inferencer *);

void print(Term *, Type *);
void print_error(Term *, error_t, char *);
