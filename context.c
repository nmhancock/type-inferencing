#include <stddef.h>

#include "inference.h"

static char *function_name = "->";

Inferencer
make_ctx(Type *types, int max_types)
{
	Inferencer ctx = {.types = types,
			  .use = 0,
			  .cap = max_types,
			  .error = NULL};
	Type *cur = &ctx.types[ctx.use];
	/* Preallocate errors to avoid that failure case. */
	if(max_types < 10) {
		ctx.use = ctx.cap = -1;
		return ctx;
	}
	*cur++ = (Type){.type = OUT_OF_TYPES,
			.undefined_symbol = NULL};
	*cur++ = (Type){.type = LOCAL_SCOPE_EXCEEDED,
			.undefined_symbol = NULL};
	*cur++ = (Type){.type = UNIFY_ERROR,
			.undefined_symbol = NULL};
	*cur++ = (Type){.type = TYPE_MISMATCH,
			.undefined_symbol = NULL};
	*cur++ = (Type){.type = RECURSIVE_UNIFICATION,
			.undefined_symbol = NULL};
	*cur++ = (Type){.type = UNDEFINED_SYMBOL,
			.undefined_symbol = NULL};
	*cur++ = (Type){.type = UNHANDLED_SYNTAX_NODE,
			.undefined_symbol = NULL};
	/* Basic types are constructed with a nullary type constructor */
	*cur++ = (Type){
		.type = OPERATOR, .op_name = "int", .types = {NULL}, .args = 0};
	*cur++ = (Type){.type = OPERATOR,
			.op_name = "bool",
			.types = {NULL},
			.args = 0};
	ctx.use = 8;
	return ctx;
}
Type *
make_type(Inferencer *ctx)
{
	if(ctx->error)
		return ctx->error;
	if(ctx->use == ctx->cap)
		return ctx->error = &ctx->types[0];
	ctx->types[ctx->use].id = ctx->use;
	return &ctx->types[ctx->use++];
}
Type *
Err(Inferencer *ctx, type_t err, char *symbol)
{
	if(ctx->error)
		return ctx->error;
	Type *error = &ctx->types[(err < 0 ? err : -1) + 7];
	error->undefined_symbol = symbol;
	return error;
}
Type *
Integer(Inferencer *ctx)
{
	if(ctx->error)
		return ctx->error;
	return &ctx->types[7];
}
Type *
Bool(Inferencer *ctx)
{
	if(ctx->error)
		return ctx->error;
	return &ctx->types[8];
}
Type *
Var(Inferencer *ctx)
{
	Type *result_type = make_type(ctx);
	if(ctx->error)
		return ctx->error;
	*result_type = (Type){
		.type = VARIABLE,
		.instance = NULL,
		.var_name = NULL,
	};
	return result_type;
}
Type *
Function(Inferencer *ctx, Type *arg_t, Type *res_t)
{
	Type *function = make_type(ctx);
	if(ctx->error)
		return ctx->error;
	/* Unrolling the constructor to keep the same form as the old code. */
	*function = (Type){
		.type = OPERATOR,
		.op_name = function_name,
		.args = 2,
		.types = {arg_t, res_t},
	};
	return function;
}
