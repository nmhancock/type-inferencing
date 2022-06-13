struct inferencing_ctx {
	int current_type;
	int max_types;
	struct lang_type *types;
};

struct inferencing_ctx make_ctx(struct lang_type *types, int max_types);
struct lang_type *make_type(struct inferencing_ctx *ctx);
struct lang_type *Function(struct inferencing_ctx *ctx, struct lang_type *arg_t,
			   struct lang_type *res_t);
struct lang_type *Var(struct inferencing_ctx *ctx);
struct lang_type *Err(struct inferencing_ctx *ctx, enum lang_type_type err,
		      char *symbol);
struct lang_type *Integer(struct inferencing_ctx *ctx);
struct lang_type *Bool(struct inferencing_ctx *ctx);
