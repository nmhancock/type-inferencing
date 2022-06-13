struct inferencing_ctx {
	int current_type;
	int max_types;
	struct term *types;
};

struct inferencing_ctx make_ctx(struct term *, int);
struct term *make_type(struct inferencing_ctx *);
struct term *Function(struct inferencing_ctx *, struct term *, struct term *);
struct term *Var(struct inferencing_ctx *);
struct term *Err(struct inferencing_ctx *, enum term_type, char *);
struct term *Integer(struct inferencing_ctx *);
struct term *Bool(struct inferencing_ctx *);
