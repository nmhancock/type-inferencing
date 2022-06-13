#include <stddef.h>

#include "inference.h"
#include "context.h"

static char *function_name = "->";

struct inferencing_ctx
make_ctx(struct term *types, int max_types)
{
	struct inferencing_ctx ctx = {.types = types,
				      .current_type = 0,
				      .max_types = max_types};
	struct term *cur = &ctx.types[ctx.current_type];
	/* Preallocate errors to avoid that failure case. */
	if(max_types < 10) {
		ctx.current_type = ctx.max_types = -1;
		return ctx;
	}
	*cur++ = (struct term){.type = OUT_OF_TYPES,
			       .undefined_symbol = NULL};
	*cur++ = (struct term){.type = LOCAL_SCOPE_EXCEEDED,
			       .undefined_symbol = NULL};
	*cur++ = (struct term){.type = UNIFY_ERROR,
			       .undefined_symbol = NULL};
	*cur++ = (struct term){.type = TYPE_MISMATCH,
			       .undefined_symbol = NULL};
	*cur++ = (struct term){.type = RECURSIVE_UNIFICATION,
			       .undefined_symbol = NULL};
	*cur++ = (struct term){.type = UNDEFINED_SYMBOL,
			       .undefined_symbol = NULL};
	*cur++ = (struct term){.type = UNHANDLED_SYNTAX_NODE,
			       .undefined_symbol = NULL};
	/* Basic types are constructed with a nullary type constructor */
	*cur++ = (struct term){
		.type = OPERATOR, .op_name = "int", .types = {NULL}, .args = 0};
	*cur++ = (struct term){.type = OPERATOR,
			       .op_name = "bool",
			       .types = {NULL},
			       .args = 0};
	ctx.current_type = 8;
	return ctx;
}
struct term *
make_type(struct inferencing_ctx *ctx)
{
	if(ctx->current_type == ctx->max_types)
		return &ctx->types[0];
	ctx->types[ctx->current_type].id = ctx->current_type;
	return &ctx->types[ctx->current_type++];
}
struct term *
Err(struct inferencing_ctx *ctx, enum term_type err,
    char *symbol)
{
	struct term *error = &ctx->types[(err < 0 ? err : -1) + 7];
	error->undefined_symbol = symbol;
	return error;
}
struct term *
Integer(struct inferencing_ctx *ctx)
{
	return &ctx->types[7];
}
struct term *
Bool(struct inferencing_ctx *ctx)
{
	return &ctx->types[8];
}
struct term *
Var(struct inferencing_ctx *ctx)
{
	struct term *result_type = make_type(ctx);
	*result_type = (struct term){
		.type = VARIABLE,
		.instance = NULL,
		.var_name = NULL,
	};
	return result_type;
}
struct term *
Function(struct inferencing_ctx *ctx, struct term *arg_t,
	 struct term *res_t)
{
	struct term *function = make_type(ctx);
	/* Unrolling the constructor to keep the same form as the old code. */
	*function = (struct term){
		.type = OPERATOR,
		.op_name = function_name,
		.args = 2,
		.types = {arg_t, res_t},
	};
	return function;
}
