#define MAX_ARGS 2

enum type_type {
	VARIABLE = 0,
	FUNCTION = 1,
	OPERATOR = 2,
	UNHANDLED_SYNTAX_NODE = -1,
	UNDEFINED_SYMBOL = -2,
	RECURSIVE_UNIFICATION = -3,
	TYPE_MISMATCH = -4,
	UNIFY_ERROR = -5,
	LOCAL_SCOPE_EXCEEDED = -6,
	OUT_OF_TYPES = -7,
};
struct type {
	union {
		struct {
			struct type *instance;
			char *var_name;
		};
		struct {
			struct type *from_type;
			struct type *to_type;
		};
		struct {
			char *op_name;
			struct type *types[MAX_ARGS];
			int args;
		};
		char *undefined_symbol;
	};
	int id;
	enum type_type type;
};
enum term_type {
	IDENTIFIER = 0,
	APPLY = 1,
	LAMBDA = 2,
	LET = 3,
	LETREC = 4
};
struct term {
	union {
		struct {
			char *name;
		};
		struct {
			struct term *fn;
			struct term *arg;
		};
		struct {
			char *v;
			struct term *defn;
			struct term *body;
		};
	};
	enum term_type type;
};

struct lt_list;
struct env {
	char *name;
	struct type *node;
	struct env *next;
};
struct inferencing_ctx;
struct type *analyze(struct inferencing_ctx *, struct term *,
		     struct env *, struct lt_list *);

void print(struct term *, struct type *);
