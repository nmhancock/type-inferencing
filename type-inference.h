#define MAX_ARGS 2

enum lang_type_type {
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
struct lang_type {
	union {
		struct {
			struct lang_type *instance;
			char *var_name;
		};
		struct {
			struct lang_type *from_type;
			struct lang_type *to_type;
		};
		struct {
			char *op_name;
			struct lang_type *types[MAX_ARGS];
			int args;
		};
		char *undefined_symbol;
	};
	int id;
	enum lang_type_type type;
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
	struct lang_type *node;
	struct env *next;
};
struct inferencing_ctx {
	int current_type;
	int max_types;
	struct lang_type *types;
};
struct inferencing_ctx make_ctx(struct lang_type *types, int max_types);
struct lang_type *analyze(struct inferencing_ctx *, struct ast_node *,
			  struct env *, struct lt_list *);

struct lang_type *make_type(struct inferencing_ctx *ctx);
struct lang_type *Function(struct inferencing_ctx *ctx, struct lang_type *arg_t,
			   struct lang_type *res_t);
struct lang_type *Var(struct inferencing_ctx *ctx);
struct lang_type *Err(struct inferencing_ctx *ctx, enum lang_type_type err,
		      char *symbol);
struct lang_type *Integer(struct inferencing_ctx *ctx);
struct lang_type *Bool(struct inferencing_ctx *ctx);

void print(struct ast_node *n, struct lang_type *t);
