#include <stddef.h>

#include "type-inference.h"
char *function_name = "->";

struct inferencing_ctx make_ctx(struct lang_type *types, int max_types)
{
	struct inferencing_ctx ctx = { .types = types,
				       .current_type = 0,
				       .max_types = max_types };
	struct lang_type *cur = &ctx.types[ctx.current_type];
	/* Preallocate errors to avoid that failure case. */
	if (max_types < 10) {
		ctx.current_type = ctx.max_types = -1;
		return ctx;
	}
	*cur++ = (struct lang_type){ .type = OUT_OF_TYPES,
				     .undefined_symbol = NULL };
	*cur++ = (struct lang_type){ .type = LOCAL_SCOPE_EXCEEDED,
				     .undefined_symbol = NULL };
	*cur++ = (struct lang_type){ .type = UNIFY_ERROR,
				     .undefined_symbol = NULL };
	*cur++ = (struct lang_type){ .type = TYPE_MISMATCH,
				     .undefined_symbol = NULL };
	*cur++ = (struct lang_type){ .type = RECURSIVE_UNIFICATION,
				     .undefined_symbol = NULL };
	*cur++ = (struct lang_type){ .type = UNDEFINED_SYMBOL,
				     .undefined_symbol = NULL };
	*cur++ = (struct lang_type){ .type = UNHANDLED_SYNTAX_NODE,
				     .undefined_symbol = NULL };
	/* Basic types are constructed with a nullary type constructor */
	*cur++ = (struct lang_type){
		.type = OPERATOR, .op_name = "int", .types = { NULL }, .args = 0
	};
	*cur++ = (struct lang_type){ .type = OPERATOR,
				     .op_name = "bool",
				     .types = { NULL },
				     .args = 0 };
	ctx.current_type = 8;
	return ctx;
}
struct lang_type *make_type(struct inferencing_ctx *ctx)
{
	if (ctx->current_type == ctx->max_types)
		return &ctx->types[0];
	ctx->types[ctx->current_type].id = ctx->current_type;
	return &ctx->types[ctx->current_type++];
}
struct lang_type *Err(struct inferencing_ctx *ctx, enum lang_type_type err,
		      char *symbol)
{
	struct lang_type *error = &ctx->types[(err < 0 ? err : -1) + 7];
	error->undefined_symbol = symbol;
	return error;
}
struct lang_type *Integer(struct inferencing_ctx *ctx)
{
	return &ctx->types[7];
}
struct lang_type *Bool(struct inferencing_ctx *ctx)
{
	return &ctx->types[8];
}
struct lang_type *Var(struct inferencing_ctx *ctx)
{
	struct lang_type *result_type = make_type(ctx);
	*result_type = (struct lang_type){
		.type = VARIABLE,
		.instance = NULL,
		.var_name = NULL,
	};
	return result_type;
}
struct lang_type *Function(struct inferencing_ctx *ctx, struct lang_type *arg_t,
			   struct lang_type *res_t)
{
	struct lang_type *function = make_type(ctx);
	/* Unrolling the constructor to keep the same form as the old code. */
	*function = (struct lang_type){
		.type = OPERATOR,
		.op_name = function_name,
		.args = 2,
		.types = { arg_t, res_t },
	};
	return function;
}
