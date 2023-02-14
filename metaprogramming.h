
#ifndef METAPROGRAMMING_H
#define METAPROGRAMMING_H

#include "parser.h"
#include "data.h"

typedef struct{
	typedecl** type_table;
	symdecl** symbol_table;
	scope*** scopestack;
	stmt*** loopstack;
	uint64_t* active_function;
	uint64_t* ntypedecls;
	uint64_t* nsymbols;
	uint64_t* nscopes;
	uint64_t* nloops; /*Needed for identifying the parent loop.*/
}seabass_builtin_ast;

/*
	Functions accessible from the compiletime metaprogramming runtime.
*/


int impl_builtin_getargc();
char** impl_builtin_getargv();
void* impl_builtin_malloc(uint64_t sz);
void* impl_builtin_realloc(char* p, uint64_t sz);
uint64_t impl_builtin_type_getsz(char* p_in);
uint64_t impl_builtin_struct_metadata(uint64_t which);
char* impl_builtin_strdup(char* s);
void impl_builtin_free(char* p);
void impl_builtin_exit(int32_t errcode);
seabass_builtin_ast* impl_builtin_get_ast();
strll* impl_builtin_peek();
void impl_builtin_puts(char* s);
void impl_builtin_gets(char* s, uint64_t sz);
int impl_builtin_open_ofile(char* fname);
void impl_builtin_close_ofile();
strll* impl_builtin_consume();
uint64_t impl_builtin_emit(char* data, uint64_t sz);
void impl_builtin_validate_function(char* p_in);
void impl_builtin_memcpy(char* a, char* b, uint64_t sz);
/* Saturday, February 11th: the blessed saturday, praise the Lord!
	functions to implement...
*/
void impl_builtin_utoa(char* buf, uint64_t v);
void impl_builtin_itoa(char* buf, int64_t v);
void impl_builtin_ftoa(char* buf, double v);
uint64_t impl_builtin_atou(char* buf);
double impl_builtin_atof(char* buf);
int64_t impl_builtin_atoi(char* buf);



int is_builtin_name(char* s);
uint64_t get_builtin_nargs(char* s);
enum{
	BUILTIN_PROTO_VOID=0,
	BUILTIN_PROTO_U8_PTR=1,
	BUILTIN_PROTO_U8_PTR2=2,
	BUILTIN_PROTO_U64_PTR=3,
	BUILTIN_PROTO_U64=4,
	BUILTIN_PROTO_I32=5,
	BUILTIN_PROTO_DOUBLE=6,
	BUILTIN_PROTO_I64=7,
};

uint64_t get_builtin_retval(char* s);
uint64_t get_builtin_arg1_type(char* s);
uint64_t get_builtin_arg2_type(char* s);
uint64_t get_builtin_arg3_type(char* s);

/*
	TODO: push built-in struct types onto the list of typedecls,
	for the compiletime execution environment to access.
*/

#endif
