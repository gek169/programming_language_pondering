
#include <stdio.h>
#include "data.h"

typedecl* type_table = NULL;
symdecl* symbol_table = NULL;
scope** scopestack = NULL;
stmt** loopstack;
uint64_t active_function = 0;
uint64_t ntypedecls = 0;
uint64_t nsymbols = 0;
uint64_t nscopes = 0;
uint64_t nloops = 0;



int scope_has_label(scope* s, char* name){
	uint64_t i;
	stmt* stmt_list = s->stmts;
	
	for(i = 0; i < s->nstmts; i++){
		if(stmt_list[i].kind == STMT_LABEL){
			if(streq(stmt_list[i].referenced_label_name,name))
				return 1;
		}
		if(stmt_list [i].myscope)
			if(scope_has_label(stmt_list [i].myscope, name))
				return 1;		
		/*if(stmt_list [i].myscope2)
			if(scope_has_label(stmt_list [i].myscope2, name))
				return 1;*/
	}
	return 0;
}


int unit_has_incomplete_symbols(){
	uint64_t i;
	for(i = 0; i < nsymbols; i++)
		if(symbol_table[i].is_incomplete) return 1;
	
	return 0;
}


void unit_throw_if_had_incomplete(){
	uint64_t i;
	for(i = 0; i < nsymbols; i++)
		if(symbol_table[i].is_incomplete)
		if(symbol_table[i].is_codegen){
			puts("The global symbol:");
			puts(symbol_table[i].name);
			puts("is codegen, and INCOMPLETE.");
			puts("Please provide a compatible definition for it.");
			parse_error("Error: Failure to define all symbols for compilation");
		}
	return;
}

/*determine base type.*/
uint64_t peek_basetype(){
	uint64_t keyw_id = ID_KEYW(peek());
	if(keyw_id){
		if(keyw_id == 2) return BASE_U8;
		if(keyw_id == 3) return BASE_I8;
		if(keyw_id == 4) return BASE_U16;
		if(keyw_id == 5) return BASE_I16;
		if(keyw_id == 6) return BASE_U32;
		if(keyw_id == 7) return BASE_I32;
		if(keyw_id == 8) return BASE_U64;
		if(keyw_id == 9) return BASE_I64;
		if(keyw_id == 10) return BASE_F32;
		if(keyw_id == 11) return BASE_F64;
		parse_error("Unknown peek_basetype keyword!");
		return 0;
	}
	if(ntypedecls > 0)
	{
		uint64_t i;
		for(i = 0; i < ntypedecls; i++)
			if(streq(type_table[i].name, peek()->text)){
				return BASE_STRUCT;
			}
	}
	parse_error("Unknown peek_basetype identifier.");
	return 0;
	/*unreachable...*/
	/*Search the functions. why not?*/
	if(nsymbols > 0){
		uint64_t i;
		for(i = 0; i < nsymbols; i++)
			if(symbol_table[i].t.is_function)
				if(streq(symbol_table[i].name, peek()->text)){
					return BASE_FUNCTION;
				}
	}
}

uint64_t consume_basetype(char* msg){
	uint64_t retval;
	require(peek_is_typename(),msg);
	retval = peek_basetype();
	consume();
	return retval;
}

int peek_is_typename(){
	if(peek() == NULL) return 0;
	if(peek()->data == TOK_KEYWORD){
		if(streq(peek()->text, "u8")) return 1;
		if(streq(peek()->text, "i8")) return 1;
		if(streq(peek()->text, "u16")) return 1;
		if(streq(peek()->text, "i16")) return 1;
		if(streq(peek()->text, "u32")) return 1;
		if(streq(peek()->text, "i32")) return 1;
		if(streq(peek()->text, "u64")) return 1;
		if(streq(peek()->text, "i64")) return 1;
		if(streq(peek()->text, "f32")) return 1;
		if(streq(peek()->text, "f64")) return 1;
		if(streq(peek()->text, "float")) return 1;
		if(streq(peek()->text, "double")) return 1;
		if(streq(peek()->text, "int")) return 1;
		if(streq(peek()->text, "sint")) return 1;
		if(streq(peek()->text, "uint")) return 1;
		if(streq(peek()->text, "byte")) return 1;
		if(streq(peek()->text, "ubyte")) return 1;
		if(streq(peek()->text, "sbyte")) return 1;
		if(streq(peek()->text, "char")) return 1;
		if(streq(peek()->text, "uchar")) return 1;
		if(streq(peek()->text, "schar")) return 1;
		if(streq(peek()->text, "short")) return 1;
		if(streq(peek()->text, "ushort")) return 1;
		if(streq(peek()->text, "sshort")) return 1;
		if(streq(peek()->text, "long")) return 1;
		if(streq(peek()->text, "ulong")) return 1;
		if(streq(peek()->text, "slong")) return 1;
		if(streq(peek()->text, "llong")) return 1;
		if(streq(peek()->text, "ullong")) return 1;
		if(streq(peek()->text, "sllong")) return 1;
		if(streq(peek()->text, "qword")) return 1;
		if(streq(peek()->text, "uqword")) return 1;
		if(streq(peek()->text, "sqword")) return 1;
	}
	if(peek()->data == TOK_IDENT)
	if(ntypedecls > 0)
	{
		uint64_t i;
		for(i = 0; i < ntypedecls; i++)
			if(streq(type_table[i].name, peek()->text)){
				return 1;
			}
	}
	return 0;
}
