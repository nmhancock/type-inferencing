#include <stddef.h>

#include "inference.h"
#include "context.h"

static char *function_name = "->";

struct inferencing_ctx
make_ctx(struct type *types, int max_types)
{
	struct inferencing_ctx ctx = {.types = types,
				      .current_type = 0,
				      .max_types = max_types};
	struct type *cur = &ctx.types[ctx.current_type];
	/* Preallocate errors to avoid that failure case. */
	if(max_types < 10) {
		ctx.current_type = ctx.max_types = -1;
		return ctx;
	}
	*cur++ = (struct type){.type = OUT_OF_TYPES,
			       .undefined_symbol = NULL};
	*cur++ = (struct type){.type = LOCAL_SCOPE_EXCEEDED,
			       .undefined_symbol = NULL};
	*cur++ = (struct type){.type = UNIFY_ERROR,
			       .undefined_symbol = NULL};
	*cur++ = (struct type){.type = TYPE_MISMATCH,
			       .undefined_symbol = NULL};
	*cur++ = (struct type){.type = RECURSIVE_UNIFICATION,
			       .undefined_symbol = NULL};
	*cur++ = (struct type){.type = UNDEFINED_SYMBOL,
			       .undefined_symbol = NULL};
	*cur++ = (struct type){.type = UNHANDLED_SYNTAX_NODE,
			       .undefined_symbol = NULL};
	/* Basic types are constructed with a nullary type constructor */
	*cur++ = (struct type){
		.type = OPERATOR, .op_name = "int", .types = {NULL}, .args = 0};
	*cur++ = (struct type){.type = OPERATOR,
			       .op_name = "bool",
			       .types = {NULL},
			       .args = 0};
	ctx.current_type = 8;
	return ctx;
}
struct type *
make_type(struct inferencing_ctx *ctx)
{
	if(ctx->current_type == ctx->max_types)
		return &ctx->types[0];
	ctx->types[ctx->current_type].id = ctx->current_type;
	return &ctx->types[ctx->current_type++];
}
struct type *
Err(struct inferencing_ctx *ctx, enum type_type err,
    char *symbol)
{
	struct type *error = &ctx->types[(err < 0 ? err : -1) + 7];
	error->undefined_symbol = symbol;
	return error;
}
struct type *
Integer(struct inferencing_ctx *ctx)
{
	return &ctx->types[7];
}
struct type *
Bool(struct inferencing_ctx *ctx)
{
	return &ctx->types[8];
}
struct type *
Var(struct inferencing_ctx *ctx)
{
	struct type *result_type = make_type(ctx);
	*result_type = (struct type){
		.type = VARIABLE,
		.instance = NULL,
		.var_name = NULL,
	};
	return result_type;
}
struct type *
Function(struct inferencing_ctx *ctx, struct type *arg_t,
	 struct type *res_t)
{
	struct type *function = make_type(ctx);
	/* Unrolling the constructor to keep the same form as the old code. */
	*function = (struct type){
		.type = OPERATOR,
		.op_name = function_name,
		.args = 2,
		.types = {arg_t, res_t},
	};
	return function;
}
