#include <stddef.h>
#include <stdint.h> /* uint32_t */

#include "inference.h"

static char *function_name = "->";
static char *apply_name = "_apply";

Type *
make_type(Inferencer *ctx)
{
	if(ctx->error) /* Leaving this check in to preserve original error */
		return NULL;
	else if(ctx->use == ctx->cap) {
		ctx->error = OUT_OF_TYPES;
		return NULL;
	} else {
		ctx->types[ctx->use].id = ctx->use;
		return &ctx->types[ctx->use++];
	}
}
Type *
Err(Inferencer *ctx, type_t err, char *symbol)
{
	if(!ctx->error) {
		ctx->error = err;
		ctx->error_msg = symbol; /* TODO: Real error messages */
	}
	return NULL;
}
Type *
Integer(Inferencer *ctx)
{
	if(ctx->error)
		return NULL;
	return &ctx->types[0];
}
Type *
Bool(Inferencer *ctx)
{
	if(ctx->error)
		return NULL;
	return &ctx->types[1];
}
Type *
Apply(Inferencer *ctx)
{
	if(ctx->error)
		return NULL;
	return &ctx->types[2];
}
Type *
Var(Inferencer *ctx)
{
	Type *var = make_type(ctx);
	if(var)
		*var = (Type){
			.type = VARIABLE,
			.generic = 1,
			.instance = NULL,
			.name = NULL,
		};
	return var;
}
Type *
Function(Inferencer *ctx, Type *arg_t, Type *res_t)
{
	Type *function = make_type(ctx);
	/* Unrolling the constructor to keep the same form as the old code. */
	if(function)
		*function = (Type){
			.type = OPERATOR,
			.name = function_name,
			.generic = arg_t->generic || res_t->generic,
			.args = 2,
			.types = {arg_t, res_t},
		};
	return function;
}
Inferencer
make_ctx(Type *types, int max_types)
{
	Inferencer ctx = {.types = types,
			  .use = 0,
			  .locals = 0,
			  .cap = max_types,
			  .error = OK};
	if(max_types < 3) {
		ctx.error = OUT_OF_TYPES;
		return ctx;
	}
	/* Basic types are constructed with a nullary type constructor */
	ctx.types[ctx.use++] = (Type){.type = OPERATOR,
				      .name = "int",
				      .types = {NULL},
				      .args = 0,
				      .id = 0};
	ctx.types[ctx.use++] = (Type){.type = OPERATOR,
				      .name = "bool",
				      .types = {NULL},
				      .args = 0,
				      .id = 1};
	ctx.types[ctx.use++] = (Type){.type = OPERATOR,
				      .name = apply_name,
				      .types = {NULL},
				      .args = 0,
				      .id = 2};
	return ctx;
}
Type *
get_result(Inferencer *ctx)
{
	return ctx->result;
}
Type *
copy_generic(Inferencer *ctx, Type *v)
{
	Type *cp = NULL;
	if(!v->generic)
		return v;
	if((cp = make_type(ctx)) == NULL)
		return NULL;
	*cp = *v;
	if(v->type == OPERATOR)
		for(size_t i = 0; i < v->args; ++i)
			cp->types[i] = copy_generic(ctx, v->types[i]);
	if(ctx->error)
		return NULL;
	cp->generic = 0;
	return cp;
}
void
var_is(Inferencer *ctx, Type *v, Type *i)
{
	*v = *i;
}
