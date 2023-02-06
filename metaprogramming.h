
#ifndef METAPROGRAMMING_H
#define METAPROGRAMMING_H

#include "parser.h"
#include "data.h"

typedef struct{
	typedecl* type_table;
	symdecl* symbol_table;
	scope** scopestack;
	stmt** loopstack;
	uint64_t active_function;
	uint64_t ntypedecls;
	uint64_t nsymbols;
	uint64_t nscopes;
	uint64_t nloops; /*Needed for identifying the parent loop.*/
}seabass_builtin_ast;

/*
	Functions accessible from the compiletime metaprogramming runtime.
*/


int impl_builtin_getargc();
char** impl_builtin_getargv();
void* impl_builtin_malloc(uint64_t sz);
char* impl_builtin_strdup(char* s);
void impl_builtin_exit(int32_t errcode);
seabass_builtin_ast* impl_builtin_get_ast();
strll* impl_builtin_peek();
void impl_builtin_puts(char* s);
void impl_builtin_gets(char* s, uint64_t sz);
int impl_builtin_open_ofile(char* fname);
int impl_builtin_close_ofile();
strll* impl_builtin_consume();
uint64_t impl_builtin_emit(char* data, uint64_t sz);
void impl_builtin_free(void* p);

int is_builtin_name(char* s);
uint64_t get_builtin_nargs(char* s);
enum{
	BUILTIN_PROTO_VOID,
	BUILTIN_PROTO_U8_PTR,
	BUILTIN_PROTO_U8_PTR2,
	BUILTIN_PROTO_U64_PTR,
	BUILTIN_PROTO_U64,
	BUILTIN_PROTO_I32
};

uint64_t get_builtin_retval(char* s);
uint64_t get_builtin_arg1_type(char* s);
uint64_t get_builtin_arg2_type(char* s);

/*
	TODO: push built-in struct types onto the list of typedecls,
	for the compiletime execution environment to access.
*/

#endif
