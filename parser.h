#ifndef PARSER_H
#define PARSER_H

#include "stringutil.h"
#include <stdint.h>
/*
	token type table
*/
#define TOK_SPACE ((void*)0)
#define TOK_NEWLINE ((void*)1)
#define TOK_STRING ((void*)2)
#define TOK_CHARLIT ((void*)3)
#define TOK_COMMENT ((void*)4)
#define TOK_MACRO ((void*)5)
#define TOK_INT_CONST ((void*)6)
#define TOK_FLOAT_CONST ((void*)7)
#define TOK_IDENT ((void*)8)
#define TOK_OPERATOR ((void*)9)
#define TOK_OCB ((void*)10)
#define TOK_CCB ((void*)11)
#define TOK_OPAREN ((void*)12)
#define TOK_CPAREN ((void*)13)
#define TOK_OBRACK ((void*)14)
#define TOK_CBRACK ((void*)15)
#define TOK_SEMIC ((void*)16)
#define TOK_UNKNOWN ((void*)17)
#define TOK_KEYWORD ((void*)18)
#define TOK_ESC_NEWLINE ((void*)19)
#define TOK_COMMA ((void*)20)
#define TOK_MACRO_OP ((void*)21)
#define TOK_INCSYS ((void*)22)
#define TOK_INCLUDE ((void*)23)
#define TOK_DEFINE ((void*)24)
#define TOK_UNDEF ((void*)25)
#define TOK_GUARD ((void*)26)

/*operator identifications*/
#define ID_OP(a) ((uint64_t)(a))
#define ID_OP2(a,b) (((uint64_t)(a)<<8)|(uint64_t)(b))
#define ID_OP3(a,b,c) (((uint64_t)(a)<<16)|((uint64_t)(b)<<8)|(uint64_t)(c))
#define ID_OP4(q,a,b,c) (((uint64_t)(q)<<32)|((uint64_t)(a)<<16)|((uint64_t)(b)<<8)|(uint64_t)(c))


static inline int ID_IS_OP1(strll* i){
	if(i->data != TOK_OPERATOR) return 0;
	if(strlen(i->text) == 1) return 1;
	return 0;
}
static inline int ID_IS_OP2(strll* i){
	if(i->data != TOK_OPERATOR) return 0;
	if(strlen(i->text) == 2) return 1;
	return 0;
}
static inline int ID_IS_OP3(strll* i){
	if(i->data != TOK_OPERATOR) return 0;
	if(strlen(i->text) == 3) return 1;
	return 0;
}
static inline int ID_IS_OP4(strll* i){
	if(i->data != TOK_OPERATOR) return 0;
	if(strlen(i->text) == 4) return 1;
	return 0;
}


static inline uint64_t ID_KEYW_STRING(const char* s){
	if(streq("fn",s)) return 40000;
	if(streq("cast",s)) return 1;
	
	if(streq("u8",s)) return 2;
	if(streq("char",s)) return 2;
	if(streq("byte",s)) return 2;
	if(streq("ubyte",s)) return 2;
	if(streq("uchar",s)) return 2;
	
	if(streq("i8",s)) return 3;
	if(streq("schar",s)) return 3;
	if(streq("sbyte",s)) return 3;
	
	if(streq("u16",s)) return 4;
	if(streq("ushort",s)) return 4;
	
	if(streq("i16",s)) return 5;
	if(streq("short",s)) return 5;
	if(streq("sshort",s)) return 5;
	
	if(streq("u32",s)) return 6;
	if(streq("uint",s)) return 6;
	if(streq("ulong",s)) return 6;
	
	if(streq("i32",s)) return 7;
	if(streq("int",s)) return 7;
	if(streq("sint",s)) return 7;
	if(streq("long",s)) return 7;
	if(streq("slong",s)) return 7;
	
	if(streq("u64",s)) return 8;
	if(streq("ullong",s)) return 8;
	if(streq("uqword",s)) return 8;
	if(streq("qword",s)) return 8;
	
	if(streq("i64",s)) return 9;
	if(streq("sllong",s)) return 9;
	if(streq("llong",s)) return 9;
	if(streq("sqword",s)) return 9;
	
	if(streq("f32",s)) return 10;
	if(streq("float",s)) return 10;
	
	if(streq("f64",s)) return 11;
	if(streq("double",s)) return 11;
	
	if(streq("break",s)) return 12;
	if(streq("data",s)) return 13;
	if(streq("string",s)) return 14;
	if(streq("end",s)) return 15;
	if(streq("continue",s)) return 16;
	if(streq("if",s)) return 17;
	if(streq("else",s)) return 18;
	if(streq("while",s)) return 19;
	if(streq("goto",s)) return 20;
	if(streq("return",s)) return 22;
	if(streq("tail",s)) return 23;
	if(streq("sizeof",s)) return 24;
	if(streq("static",s)) return 25;
	
	if(streq("pub",s)) return 26;
	if(streq("public",s)) return 26;
	
	if(streq("struct",s)) return 27;
	if(streq("class",s)) return 27;
	if(streq("asm",s)) return 28;
	if(streq("method",s)) return 29;
	if(streq("predecl",s)) return 30;
	if(streq("codegen",s)) return 31;
	//constexpr
	if(streq("constexpri",s)) return 32;
	if(streq("constexprf",s)) return 33;
	if(streq("switch",s)) return 34;
	if(streq("for",s)) return 35;
	if(streq("elif",s)) return 36;
	if(streq("elseif",s)) return 37;
	puts("Internal Error: Unknown keyw_string, add it:");
	puts(s);
	exit(1);
	return 0;
}

static inline uint64_t ID_KEYW(strll* s){
	if(s->data != TOK_KEYWORD) return 0;
	return ID_KEYW_STRING(s->text);
}


extern int peek_always_not_null;
/*Used for automatically generating symbols for anonymous data.*/
extern uint64_t symbol_generator_count;
char* gen_reserved_sym_name();

extern int saved_argc;
extern char** saved_argv;

strll* peek();
/*lookahead by n*/
strll* peekn(unsigned n);
strll* consume();
void parse_error(char* msg);
void require(int cond, char* msg);
int peek_is_operator();
int peek_is_semic();
strll* consume_semicolon(char* msg);
void require_peek_notnull(char* msg);

/*global statement parsers*/
void parse_gvardecl();
void parse_datastmt();
void parse_structdecl();
/*struct-level parsing*/
void parse_struct_member(uint64_t sid); /*type ident*/

/*Specific functions provided by other files...*/

int peek_is_typename();
uint64_t peek_basetype();
uint64_t consume_basetype(char* msg);
/*Consumption*/

int64_t parse_cexpr_int();
double parse_cexpr_double();
void unit_throw_if_had_incomplete();
uint64_t parse_stringliteral();
void parse_continue_break();
void parse_fn(int is_method);
void parse_method();
void parse_fbody();
void parse_stmts();
void parse_stmt();


/*safe memory allocation*/
void* m_allocX(uint64_t sz);
void* c_allocX(uint64_t sz);
void* re_allocX(void* p, uint64_t sz);

void consume_keyword(char* s);





#endif
