/* Usage examples */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "inference.h"

#define MAX_TYPES 200
int
main(void)
{
	Type types[MAX_TYPES];
	Inferencer ctx = make_ctx(types, MAX_TYPES);

	Type *var1 = Var(&ctx);
	Type *var2 = Var(&ctx);
	Type *pair_type = make_type(&ctx);
	*pair_type = (Type){.type = OPERATOR,
			    .name = "*",
			    .args = 2,
			    .types = {var1, var2}};
	Type *var3 = Var(&ctx);

	Env envs[7] = {
		{.name = "pair",
		 .node = Function(&ctx, var1, Function(&ctx, var2, pair_type)),
		 .next = &envs[1]},
		{.name = "true", .node = Bool(&ctx), .next = &envs[2]},
		{
			.name = "cond",
			.node = Function(&ctx, Bool(&ctx),
					 Function(&ctx, var3,
						  Function(&ctx, var3, var3))),
			.next = &envs[3],
		},
		{.name = "zero",
		 .node = Function(&ctx, Integer(&ctx), Bool(&ctx)),
		 .next = &envs[4]},
		{.name = "pred",
		 .node = Function(&ctx, Integer(&ctx), Integer(&ctx)),
		 .next = &envs[5]},
		{.name = "times",
		 .node = Function(&ctx, Integer(&ctx),
				  Function(&ctx, Integer(&ctx), Integer(&ctx))),
		 .next = &envs[6]},
		{.name = "factorial",
		 .node = Function(&ctx, Integer(&ctx), Integer(&ctx)),
		 .next = NULL}};
	Env *my_env = envs;

	Term factorial = {
		.type = LETREC,
		.v = "factorial",
		.defn = &(Term){
			.type = LAMBDA,
			.v = "n",
			.body = &(Term){
				.type = APPLY,
				.fn = &(Term){
					.type = APPLY,
					.fn = &(Term){
						.type = APPLY,
						.fn = &(Term){
							.type = IDENTIFIER,
							.name = "cond"},
						.arg = &(Term){.type = APPLY, .fn = &(Term){.type = IDENTIFIER, .name = "zero"}, .arg = &(Term){.type = IDENTIFIER, .name = "n"}}},
					.arg = &(Term){.type = IDENTIFIER, .name = "1"}},
				.arg = &(Term){.type = APPLY, .fn = &(Term){.type = APPLY, .fn = &(Term){.type = IDENTIFIER, .name = "times"}, .arg = &(Term){.type = IDENTIFIER, .name = "n"}}, .arg = &(Term){.type = APPLY, .fn = &(Term){.type = IDENTIFIER, .name = "factorial"}, .arg = &(Term){.type = APPLY, .fn = &(Term){.type = IDENTIFIER, .name = "pred"}, .arg = &(Term){.type = IDENTIFIER, .name = "n"}}}}}},
		.body = &(Term){.type = APPLY, .fn = &(Term){.type = IDENTIFIER, .name = "factorial"}, .arg = &(Term){.type = IDENTIFIER, .name = "5"}}};

	Type *t = NULL;
	printf("ctx.use: %d\n", ctx.use);
	clock_t total = 0;
#define ITERATIONS 1000000
	for(int i = 0; i < ITERATIONS; ++i) {
		ctx.use = 22; /* Experimentally determined */
		clock_t tic = clock();
		(void)extern_analyze(&ctx, &factorial, my_env, NULL);
		clock_t toc = clock();
		total += toc - tic;
		if(ctx.error)
			break;
	}
	if(ctx.error) {
		print_error(&factorial, ctx.error, ctx.error_msg);
		return 1;
	}
	t = get_result(&ctx);
	fprintf(stdout, "Iterations: %d Total time: %f ns\n", ITERATIONS,
		(double)(total / CLOCKS_PER_SEC * 1000000));
	print(&factorial, t);
	printf("DEBUG: %d\n", ctx.use);
	return 0;
}
