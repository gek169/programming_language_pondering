

#include "metaprogramming.h"

#include "stringutil.h"
#include "targspecifics.h"

#include "libmin.h"
#include "parser.h"
#include "data.h"
#include "metaprogramming.h"
#include "astexec.h"
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

void* impl_builtin_realloc(char* p, uint64_t sz){
	 p = realloc(p, sz);
	if(!p) {puts("<BUILTIN ERROR> malloc failed.");exit(1);}
	return p;
}

uint64_t impl_builtin_type_getsz(char* p_in){
	type* p = (type*)p_in;
	return type_getsz(*p);
}

uint64_t impl_builtin_struct_metadata(uint64_t which){
	if(which == 0)
		return sizeof(type);
	if(which == 1)
		return sizeof(typedecl);
	if(which == 2)
		return sizeof(symdecl);
	if(which == 3)
		return sizeof(scope);
	if(which == 4)
		return sizeof(stmt);
	if(which == 5)
		return sizeof(expr_node);
	if(which == 6)
		return sizeof(seabass_builtin_ast);
	return 0;
}

char* impl_builtin_strdup(char* s){
	char* p = strdup(s);
	if(!p) {puts("<BUILTIN ERROR> malloc failed.");exit(1);}
	return p;
}

void impl_builtin_free(char* p){
	free(p);
}

void impl_builtin_exit(int32_t errcode){
	exit(errcode);
}

//returns an owning pointer.
seabass_builtin_ast* impl_builtin_get_ast(){
	seabass_builtin_ast* retval = malloc(sizeof(seabass_builtin_ast));
	if(retval == NULL){puts("<BUILTIN ERROR> malloc failed.");exit(1);}

	retval->type_table = &type_table;
	retval->symbol_table = &symbol_table;
	retval->scopestack = &scopestack;
	retval->loopstack = &loopstack;
	retval->active_function = &active_function;

	retval->nsymbols = &nsymbols;
	retval->nscopes = &nscopes;
	retval->ntypedecls = &ntypedecls;
	retval->nloops = &nloops;
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

void impl_builtin_close_ofile(){
	if(!ofile) return;
	if(ofile)fclose(ofile);
	ofile = NULL;
	return;
}

strll* impl_builtin_consume(){
	return consume();
}

uint64_t impl_builtin_emit(char* data, uint64_t sz){
	if(!ofile)
	{
		puts("<BUILTIN ERROR> Output file not opened. Cannot emit anything.");
		exit(1);
	}
	return fwrite(data, 1, sz, ofile);
}

void validate_function(symdecl* funk);
void impl_builtin_validate_function(char* p_in){
	//save the environment so that we can continue executing after validate_function has been invoked.
	uint64_t active_function_saved = active_function;
	uint64_t nscopes_saved = nscopes;
	uint64_t nloops_saved = nloops;
	scope** scopestack_saved = scopestack;
	stmt** loopstack_saved = loopstack;
		scopestack = NULL;
		nscopes = 0;
		nloops = 0;
		loopstack = NULL;
		symdecl* funk = (symdecl*)p_in;
		validate_function(funk);
	
	if(scopestack) free(scopestack);
	active_function = active_function_saved;
	scopestack = scopestack_saved;
	nscopes = nscopes_saved;
	nloops = nloops_saved;
	loopstack = loopstack_saved;
}

void impl_builtin_memcpy(char* a, char* b, uint64_t sz){
	memcpy(a,b,sz);
}

void impl_builtin_utoa(char* buf, uint64_t v){
	mutoa(buf,v);
}
void impl_builtin_itoa(char* buf, int64_t v){
	mitoa(buf,v);
}
/*TODO: make proper implementation of ftoa with scientific notation and what have you.*/
void impl_builtin_ftoa(char* buf, double v){
	//sprintf(buf, "%f", v);
	mftoa(buf, v, 64);
}
uint64_t impl_builtin_atou(char* buf){
	return matou(buf);
}
double impl_builtin_atof(char* buf){
	return matof(buf);
}
int64_t impl_builtin_atoi(char* buf){
	return matoi(buf);
}



/*
	AST manipulation and analysis...
*/
int32_t impl_builtin_peek_is_fname(){
	return peek_is_fname();
}
int32_t impl_builtin_str_is_fname(char* s){
	return str_is_fname(s);
} 

void impl_builtin_scopestack_push(char* scopeptr){
	scopestack_push((scope*)scopeptr);
}
void impl_builtin_scopestack_pop(){
	scopestack_pop();
}

void impl_builtin_loopstack_push(char* stmtptr){
	loopstack_push((stmt*)stmtptr);
}
void impl_builtin_loopstack_pop(){
	loopstack_pop();
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
	if(streq(s, "__builtin_realloc")) return 1;
	if(streq(s, "__builtin_type_getsz")) return 1;
	if(streq(s, "__builtin_struct_metadata")) return 1;
	if(streq(s, "__builtin_validate_function")) return 1;
	if(streq(s, "__builtin_memcpy")) return 1;
	/*
		The blessed saturday
	*/
	if(streq(s, "__builtin_utoa")) return 1;
	if(streq(s, "__builtin_itoa")) return 1;
	if(streq(s, "__builtin_ftoa")) return 1;
	
	if(streq(s, "__builtin_atof")) return 1;
	if(streq(s, "__builtin_atou")) return 1;
	if(streq(s, "__builtin_atoi")) return 1;
	
	if(streq(s, "__builtin_peek_is_fname")) return 1;
	if(streq(s, "__builtin_str_is_fname")) return 1;
	return 0;
}

uint64_t get_builtin_nargs(char* s){
	if(streq(s, "__builtin_emit")) return 2;
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
	if(streq(s, "__builtin_realloc")) return 2;
	if(streq(s, "__builtin_struct_metadata")) return 1;
	if(streq(s, "__builtin_validate_function")) return 1;
	if(streq(s, "__builtin_memcpy")) return 3;
	//these take a string and a value.
	if(streq(s, "__builtin_utoa")) return 2;
	if(streq(s, "__builtin_itoa")) return 2;
	if(streq(s, "__builtin_ftoa")) return 2;
	//these just take a string.
	if(streq(s, "__builtin_atof")) return 1;
	if(streq(s, "__builtin_atou")) return 1;
	if(streq(s, "__builtin_atoi")) return 1;

	if(streq(s, "__builtin_peek_is_fname")) return 0;
	if(streq(s, "__builtin_str_is_fname")) return 1;
	
	return 0;
}

uint64_t get_builtin_retval(char* s){
	if(streq(s, "__builtin_emit")) return BUILTIN_PROTO_VOID;
	if(streq(s, "__builtin_open_ofile")) return BUILTIN_PROTO_I32;
	if(streq(s, "__builtin_close_ofile")) return BUILTIN_PROTO_VOID;
	if(streq(s, "__builtin_get_ast")) return BUILTIN_PROTO_U64_PTR;
	if(streq(s, "__builtin_consume")) return BUILTIN_PROTO_U64_PTR;
	if(streq(s, "__builtin_gets")) return BUILTIN_PROTO_VOID;
	if(streq(s, "__builtin_puts")) return BUILTIN_PROTO_VOID;
	if(streq(s, "__builtin_peek")) return BUILTIN_PROTO_U64_PTR;
	if(streq(s, "__builtin_exit")) return BUILTIN_PROTO_VOID;
	if(streq(s, "__builtin_strdup")) return BUILTIN_PROTO_U8_PTR;
	if(streq(s, "__builtin_malloc")) return BUILTIN_PROTO_U8_PTR;
	if(streq(s, "__builtin_getargv")) return BUILTIN_PROTO_U8_PTR2;
	if(streq(s, "__builtin_getargc")) return BUILTIN_PROTO_I32;
	if(streq(s, "__builtin_free")) return BUILTIN_PROTO_VOID;
	if(streq(s, "__builtin_realloc")) return BUILTIN_PROTO_U8_PTR;
	if(streq(s, "__builtin_type_getsz")) return BUILTIN_PROTO_U64;
	if(streq(s, "__builtin_struct_metadata")) return BUILTIN_PROTO_U64;
	if(streq(s, "__builtin_validate_function")) return BUILTIN_PROTO_VOID;
	if(streq(s, "__builtin_memcpy")) return BUILTIN_PROTO_VOID;
	
	if(streq(s, "__builtin_utoa")) return BUILTIN_PROTO_VOID;
	if(streq(s, "__builtin_itoa")) return BUILTIN_PROTO_VOID;
	if(streq(s, "__builtin_ftoa")) return BUILTIN_PROTO_VOID;
	
	if(streq(s, "__builtin_atof")) return BUILTIN_PROTO_DOUBLE;
	if(streq(s, "__builtin_atou")) return BUILTIN_PROTO_U64;
	if(streq(s, "__builtin_atoi")) return BUILTIN_PROTO_I64;

	if(streq(s, "__builtin_peek_is_fname")) return BUILTIN_PROTO_I32;
	if(streq(s, "__builtin_str_is_fname")) return BUILTIN_PROTO_I32;
	
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
	if(streq(s, "__builtin_free")) return BUILTIN_PROTO_U8_PTR;
	if(streq(s, "__builtin_realloc")) return BUILTIN_PROTO_U8_PTR;
	if(streq(s, "__builtin_type_getsz")) return BUILTIN_PROTO_U8_PTR;
	if(streq(s, "__builtin_struct_metadata")) return BUILTIN_PROTO_U64;
	if(streq(s, "__builtin_memcpy")) return BUILTIN_PROTO_U8_PTR;

	//all of them take a char* as the first argument.
	if(streq(s, "__builtin_utoa")) return BUILTIN_PROTO_U8_PTR;
	if(streq(s, "__builtin_itoa")) return BUILTIN_PROTO_U8_PTR;
	if(streq(s, "__builtin_ftoa")) return BUILTIN_PROTO_U8_PTR;

	if(streq(s, "__builtin_atof")) return BUILTIN_PROTO_U8_PTR;
	if(streq(s, "__builtin_atou")) return BUILTIN_PROTO_U8_PTR;
	if(streq(s, "__builtin_atoi")) return BUILTIN_PROTO_U8_PTR;

	if(streq(s, "__builtin_str_is_fname")) return BUILTIN_PROTO_U8_PTR;
	return 0;
}
uint64_t get_builtin_arg2_type(char* s){
	if(streq(s, "__builtin_emit")) return BUILTIN_PROTO_U64;
	if(streq(s, "__builtin_gets")) return BUILTIN_PROTO_U64;
	if(streq(s, "__builtin_realloc")) return BUILTIN_PROTO_U64;
	if(streq(s, "__builtin_memcpy")) return BUILTIN_PROTO_U8_PTR;

	//these functions take "what to convert" as the second argument.
	if(streq(s, "__builtin_utoa")) return BUILTIN_PROTO_U64;
	if(streq(s, "__builtin_itoa")) return BUILTIN_PROTO_I64;
	if(streq(s, "__builtin_ftoa")) return BUILTIN_PROTO_DOUBLE;
	
	//if(streq(s, "__builtin_atof")) return BUILTIN_PROTO_U8_PTR;
	//if(streq(s, "__builtin_atou")) return BUILTIN_PROTO_U8_PTR;
	//if(streq(s, "__builtin_atoi")) return BUILTIN_PROTO_U8_PTR;
	return 0;
}
uint64_t get_builtin_arg3_type(char* s){
	if(streq(s, "__builtin_memcpy")) return BUILTIN_PROTO_U64;
	return 0;
}






