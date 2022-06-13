struct inferencing_ctx {
	int current_type;
	int max_types;
	struct type *types;
};

struct inferencing_ctx make_ctx(struct type *, int);
struct type *make_type(struct inferencing_ctx *);
struct type *Function(struct inferencing_ctx *, struct type *, struct type *);
struct type *Var(struct inferencing_ctx *);
struct type *Err(struct inferencing_ctx *, enum type_type, char *);
struct type *Integer(struct inferencing_ctx *);
struct type *Bool(struct inferencing_ctx *);
