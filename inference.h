#define MAX_ARGS 2

enum term_type {
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
struct term {
	union {
		struct {
			struct term *instance;
			char *var_name;
		};
		struct {
			struct term *from_type;
			struct term *to_type;
		};
		struct {
			char *op_name;
			struct term *types[MAX_ARGS];
			int args;
		};
		char *undefined_symbol;
	};
	int id;
	enum term_type type;
};
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
			char *name;
		};
		struct {
			struct ast_node *fn;
			struct ast_node *arg;
		};
		struct {
			char *v;
			struct ast_node *defn;
			struct ast_node *body;
		};
	};
	enum ast_node_type type;
};

struct lt_list;
struct env {
	char *name;
	struct term *node;
	struct env *next;
};
struct inferencing_ctx;
struct term *analyze(struct inferencing_ctx *, struct ast_node *,
			  struct env *, struct lt_list *);

void print(struct ast_node *n, struct term *t);
