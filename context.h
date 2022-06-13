struct inferencing_ctx {
	int current_type;
	int max_types;
	struct term *types;
};

struct inferencing_ctx make_ctx(struct term *types, int max_types);
struct term *make_type(struct inferencing_ctx *ctx);
struct term *Function(struct inferencing_ctx *ctx, struct term *arg_t,
		      struct term *res_t);
struct term *Var(struct inferencing_ctx *ctx);
struct term *Err(struct inferencing_ctx *ctx, enum term_type err,
		 char *symbol);
struct term *Integer(struct inferencing_ctx *ctx);
struct term *Bool(struct inferencing_ctx *ctx);
