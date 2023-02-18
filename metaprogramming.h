
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


int impl_builtin_getargc(); //DONE
char** impl_builtin_getargv(); //DONE
void* impl_builtin_malloc(uint64_t sz); //DONE
void* impl_builtin_realloc(char* p, uint64_t sz); //DONE
uint64_t impl_builtin_type_getsz(char* p_in); //DONE
uint64_t impl_builtin_struct_metadata(uint64_t which); //DONE
char* impl_builtin_strdup(char* s); //DONE
void impl_builtin_free(char* p); //DONE
void impl_builtin_exit(int32_t errcode); //DONE
seabass_builtin_ast* impl_builtin_get_ast(); //DONE
strll* impl_builtin_peek(); //DONE
void impl_builtin_puts(char* s); //DONE
void impl_builtin_gets(char* s, uint64_t sz); //DONE
int impl_builtin_open_ofile(char* fname); //DONE
void impl_builtin_close_ofile(); //DONE
strll* impl_builtin_consume(); //DONE
uint64_t impl_builtin_emit(char* data, uint64_t sz); //DONE
void impl_builtin_validate_function(char* p_in); //DONE
void impl_builtin_memcpy(char* a, char* b, uint64_t sz); //DONE
/* Saturday, February 11th: the blessed saturday, praise the Lord!
	functions to implement...
*/
void impl_builtin_utoa(char* buf, uint64_t v); //DONE
void impl_builtin_itoa(char* buf, int64_t v); //DONE
void impl_builtin_ftoa(char* buf, double v); //DONE
uint64_t impl_builtin_atou(char* buf); //DONE
double impl_builtin_atof(char* buf); //DONE
int64_t impl_builtin_atoi(char* buf); //DONE

/* AST manipulation functions.
	
*/
int32_t impl_builtin_peek_is_fname(); //TODO
int32_t impl_builtin_str_is_fname(char *s); //TODO

void impl_builtin_scopestack_push(char* scopeptr);
void impl_builtin_scopestack_pop();

void impl_builtin_loopstack_push(char* stmtptr);
void impl_builtin_loopstack_pop();
char* impl_builtin_parser_push_statement();




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
