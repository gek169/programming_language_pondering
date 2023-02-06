

#include "metaprogramming.h"

/*
	Implementations for the builtin functions.
*/

FILE* ofile = NULL;

int impl_builtin_getargc(){return saved_argc;}
char** impl_builtin_getargv(){return saved_argv;}

void* impl_builtin_malloc(uint64_t sz){
	void* p = calloc(sz,1);
	if(!p) {puts("<BUILTIN ERROR> malloc failed.");exit(1);}
	return p;
}

char* impl_builtin_strdup(char* s){
	char* p = strdup(s);
	if(!p) {puts("<BUILTIN ERROR> malloc failed.");exit(1);}
	return p;
}

void impl_builtin_free(void* p){
	free(p);
}

void impl_builtin_exit(int32_t errcode){
	exit(errcode);
}

//returns an owning pointer.
seabass_builtin_ast* impl_builtin_get_ast(){
	seabass_builtin_ast* retval = malloc(sizeof(seabass_builtin_ast));
	if(retval == NULL){puts("<BUILTIN ERROR> malloc failed.");exit(1);}
	retval->type_table = type_table;
	retval->ntypedecls = ntypedecls;
	retval->symbol_table = symbol_table;
	retval->nsymbols = nsymbols;
	retval->scopestack = scopestack;
	retval->nscopes = nscopes;
	retval->active_function = active_function;
	retval->nloops = nloops;
	retval->loopstack = loopstack;
	return retval;
}
strll* impl_builtin_peek(){
	return peek();
}

void impl_builtin_puts(char* s){
	puts(s);
}

void impl_builtin_gets(char* s, uint64_t sz){
	fread(s, sz, 1, stdin);
}

int impl_builtin_open_ofile(char* fname){
	ofile = fopen(fname,"wb");
	return ofile != NULL;
}

int impl_builtin_close_ofile(){
	if(!ofile) return 0;
	if(ofile)fclose(ofile);
	ofile = NULL;
	return 1;
}

strll* impl_builtin_consume(){
	return consume();
}

uint64_t impl_builtin_emit(char* data, uint64_t sz){
	if(!ofile)
	{
		puts("<BUILTIN ERROR> Output file not open. Cannot emit anything.");
		exit(1);
	}
	return fwrite(data, 1, sz, ofile);
}


int is_builtin_name(char* s){
	if(streq(s, "__builtin_emit")) return 1;
	if(streq(s, "__builtin_open_ofile")) return 1;
	if(streq(s, "__builtin_close_ofile")) return 1;
	if(streq(s, "__builtin_get_ast")) return 1;
	if(streq(s, "__builtin_consume")) return 1;
	if(streq(s, "__builtin_gets")) return 1;
	if(streq(s, "__builtin_puts")) return 1;
	if(streq(s, "__builtin_peek")) return 1;
	if(streq(s, "__builtin_exit")) return 1;
	if(streq(s, "__builtin_strdup")) return 1;
	if(streq(s, "__builtin_malloc")) return 1;
	if(streq(s, "__builtin_getargv")) return 1;
	if(streq(s, "__builtin_getargc")) return 1;
	if(streq(s, "__builtin_free")) return 1;
	return 0;
}

uint64_t get_builtin_nargs(char* s){
	if(streq(s, "__builtin_emit")) return 1;
	if(streq(s, "__builtin_open_ofile")) return 1;
	if(streq(s, "__builtin_close_ofile")) return 0;
	if(streq(s, "__builtin_get_ast")) return 0;
	if(streq(s, "__builtin_consume")) return 0;
	if(streq(s, "__builtin_gets")) return 2;
	if(streq(s, "__builtin_puts")) return 1;
	if(streq(s, "__builtin_peek")) return 0;
	if(streq(s, "__builtin_exit")) return 1;
	if(streq(s, "__builtin_strdup")) return 1;
	if(streq(s, "__builtin_malloc")) return 1;
	if(streq(s, "__builtin_getargv")) return 0;
	if(streq(s, "__builtin_getargc")) return 0;
	if(streq(s, "__builtin_free")) return 1;
	return 0;
}

uint64_t get_builtin_retval(char* s){
	if(streq(s, "__builtin_emit")) return BUILTIN_PROTO_VOID;
	if(streq(s, "__builtin_open_ofile")) return BUILTIN_PROTO_VOID;
	if(streq(s, "__builtin_close_ofile")) return BUILTIN_PROTO_VOID;
	if(streq(s, "__builtin_get_ast")) return BUILTIN_PROTO_U64_PTR;
	if(streq(s, "__builtin_consume")) return BUILTIN_PROTO_U64_PTR;
	if(streq(s, "__builtin_gets")) return BUILTIN_PROTO_U8_PTR;
	if(streq(s, "__builtin_puts")) return BUILTIN_PROTO_VOID;
	if(streq(s, "__builtin_peek")) return BUILTIN_PROTO_U64_PTR;
	if(streq(s, "__builtin_exit")) return BUILTIN_PROTO_VOID;
	if(streq(s, "__builtin_strdup")) return BUILTIN_PROTO_U8_PTR;
	if(streq(s, "__builtin_malloc")) return BUILTIN_PROTO_U8_PTR;
	if(streq(s, "__builtin_getargv")) return BUILTIN_PROTO_U8_PTR2;
	if(streq(s, "__builtin_getargc")) return BUILTIN_PROTO_I32;
	if(streq(s, "__builtin_free")) return 1;
	return 0;
}

uint64_t get_builtin_arg1_type(char* s){
	if(streq(s, "__builtin_emit")) return BUILTIN_PROTO_U8_PTR;
	if(streq(s, "__builtin_open_ofile")) return BUILTIN_PROTO_U8_PTR;
	if(streq(s, "__builtin_gets")) return BUILTIN_PROTO_U8_PTR;
	if(streq(s, "__builtin_puts")) return BUILTIN_PROTO_U8_PTR;
	if(streq(s, "__builtin_exit")) return BUILTIN_PROTO_I32;
	if(streq(s, "__builtin_strdup")) return BUILTIN_PROTO_U8_PTR;
	if(streq(s, "__builtin_malloc")) return BUILTIN_PROTO_U8_PTR;
	if(streq(s, "__builtin_free")) return 1;
	return 0;
}
uint64_t get_builtin_arg2_type(char* s){
	if(streq(s, "__builtin_gets")) return BUILTIN_PROTO_U64;
	return 0;
}






