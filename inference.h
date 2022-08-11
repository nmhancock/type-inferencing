/* requires stdint.h */
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
	MAX_RECURSION_EXCEEDED = -8,
} error_t;
typedef enum {
	VARIABLE = 0,
	OPERATOR = 1,
} type_t;
typedef struct Type {
	char *name;
	union {
		struct Type *instance;
		struct {
			struct Type *types[MAX_ARGS];
			uint32_t args : 2;
		};
	};
	uint32_t generic : 1;
	uint32_t type : 1;
	uint32_t id : 28;
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
	int locals;
} Inferencer;
error_t analyze(Inferencer *ctx, Term *node, Env *env);
Type *get_result(Inferencer *ctx);

Inferencer make_ctx(Type *, int); /* TODO: Fix return type to be int */
Type *make_type(Inferencer *);
Type *Function(Inferencer *, Type *, Type *);
Type *Var(Inferencer *);
Type *Err(Inferencer *, type_t, char *);
Type *Integer(Inferencer *);
Type *Bool(Inferencer *);
Type *Apply(Inferencer *);
Type *copy_generic(Inferencer *ctx, Type *v);
void var_is(Inferencer *ctx, Type *v, Type *i);

void print(Term *, Type *);
void print_error(Term *, error_t, char *);
