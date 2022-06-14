typedef struct Inferencer {
	int current_type;
	int max_types;
	Type *types;
} Inferencer;

Inferencer make_ctx(Type *, int);
Type *make_type(Inferencer *);
Type *Function(Inferencer *, Type *, Type *);
Type *Var(Inferencer *);
Type *Err(Inferencer *, type_t, char *);
Type *Integer(Inferencer *);
Type *Bool(Inferencer *);
