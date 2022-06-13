/* Usage examples */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "inference.h"
#include "context.h"

#define MAX_TYPES 200
int
main(void)
{
	struct lang_type types[MAX_TYPES];
	struct inferencing_ctx ctx = make_ctx(types, MAX_TYPES);

	struct lang_type *var1 = Var(&ctx);
	struct lang_type *var2 = Var(&ctx);
	struct lang_type *pair_type = make_type(&ctx);
	*pair_type = (struct lang_type){.type = OPERATOR,
					.op_name = "*",
					.args = 2,
					.types = {var1, var2}};
	struct lang_type *var3 = Var(&ctx);

	struct env envs[7] = {
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
	struct env *my_env = envs;

	struct ast_node factorial = {.type = LETREC,
				     .v = "factorial",
				     .defn =
					     &(struct ast_node){
						     .type = LAMBDA,
						     .v = "n",
						     .body =
							     &(struct ast_node){
								     .type = APPLY,
								     .fn =
									     &(
										     struct ast_node){.type = APPLY,
												      .fn =
													      &(struct ast_node){.type = APPLY,
																 .fn =
																	 &(struct ast_node){
																		 .type = IDENTIFIER,
																		 .name = "cond"},
																 .arg =
																	 &(struct ast_node){
																		 .type = APPLY,
																		 .fn = &(struct
																			 ast_node){.type = IDENTIFIER,
																				   .name = "zero"},
																		 .arg = &(
																			 struct
																			 ast_node){.type = IDENTIFIER, .name = "n"}}},
												      .arg =
													      &(struct ast_node){
														      .type = IDENTIFIER,
														      .name = "1"}},
								     .arg = &(struct ast_node){.type = APPLY,
											       .fn =
												       &(struct ast_node){
													       .type = APPLY,
													       .fn =
														       &(struct
															 ast_node){.type = IDENTIFIER,
																   .name = "times"},
													       .arg =
														       &(struct ast_node){
															       .type = IDENTIFIER,
															       .name = "n"},
												       },
											       .arg = &(struct
													ast_node){.type = APPLY,
														  .fn = &(struct ast_node){.type = IDENTIFIER, .name = "factorial"},
														  .arg =
															  &(struct ast_node){
																  .type = APPLY,
																  .fn =
																	  &(struct
																	    ast_node){.type = IDENTIFIER,
																		      .name = "pred"},
																  .arg =
																	  &(struct ast_node){.type = IDENTIFIER,
																			     .name = "n"},
															  }}}},
					     },
				     .body = &(struct ast_node){
					     .type = APPLY,
					     .fn =
						     &(struct ast_node){
							     .type = IDENTIFIER,
							     .name = "factorial"},
					     .arg =
						     &(struct ast_node){
							     .type = IDENTIFIER,
							     .name = "5"},
				     }};

	struct lang_type *t;
	printf("ctx.current_type: %d\n", ctx.current_type);
	clock_t total = 0;
#define ITERATIONS 10000000
	for(int i = 0; i < ITERATIONS; ++i) {
		ctx.current_type = 22; /* Experimentally determined */
		clock_t tic = clock();
		t = analyze(&ctx, &factorial, my_env, NULL);
		clock_t toc = clock();
		total += toc - tic;
	}
	fprintf(stdout, "Iterations: %d Total time: %f ns\n", ITERATIONS,
		(double)(total / CLOCKS_PER_SEC * 1000000));
	print(&factorial, t);
	printf("DEBUG: %d\n", ctx.current_type);
	return 0;
}
