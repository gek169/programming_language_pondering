



#include "stringutil.h"
#include "targspecifics.h"

#include "libmin.h"
#include "parser.h"
#include "data.h"
#include "metaprogramming.h"
#include "astexec.h"


static void do_indent(uint64_t indentlevel){
	while(indentlevel){
		fputs(" ",stdout);
		indentlevel--;
	}
}

static void astdump_print_type(type t, int should_print_isfunction){
	char buf[80];
	uint64_t c; //grabbed pointerlevel
	if(t.basetype == BASE_VOID)fputs("void",stdout);
	if(t.basetype == BASE_U8)fputs("u8",stdout);
	if(t.basetype == BASE_I8)fputs("i8",stdout);	

	if(t.basetype == BASE_U16)fputs("u16",stdout);
	if(t.basetype == BASE_I16)fputs("i16",stdout);
	
	if(t.basetype == BASE_U32)fputs("u32",stdout);
	if(t.basetype == BASE_I32)fputs("i32",stdout);
		
	if(t.basetype == BASE_U64)fputs("u64",stdout);
	if(t.basetype == BASE_I64)fputs("i64",stdout);
	if(t.basetype == BASE_F64)fputs("f64",stdout);
	if(t.basetype == BASE_F32)fputs("f32",stdout);
	if(t.basetype == BASE_FUNCTION)fputs("base_function(error)",stdout);
	if(t.basetype > BASE_FUNCTION)fputs("base_error",stdout);

	if(t.basetype == BASE_STRUCT){
		fputs("struct(",stdout);
		fputs(type_table[t.structid].name,stdout);
		fputs(")",stdout);
	}
	if(t.arraylen){
		fputs("[",stdout);
		mutoa(buf, t.arraylen);
		fputs(buf,stdout);
		fputs("]",stdout);
	}
	c = t.pointerlevel;
	while(c){
		fputs("*",stdout);
		c--;
	}
	fputs(" ",stdout);
	if(should_print_isfunction)
		if(t.is_function)
			fputs("[is_function] ",stdout);
	if(t.membername){
		fputs("membername:",stdout);
		fputs(t.membername,stdout);
		fputs(" ",stdout);
	}
}

static void astdump_printsymbol(symdecl* s, 
	uint64_t indentlevel, 
	int should_print_isfunction
){
	char buf[80];
	fputs("\n",stdout);
	do_indent(indentlevel);
	if(s->t.is_function) fputs("fn ",stdout);
	if(!s->t.is_function) fputs("var ",stdout);
	if(s->is_codegen) fputs ("codegen ",stdout);
	if(s->is_pub) fputs("pub ",stdout);
	if(s->is_impure_globals_or_asm) fputs("known-impure ",stdout);
	if(s->is_incomplete) fputs("incomplete ",stdout);
	if(s->cdata != NULL) fputs("has-cdata ",stdout);
	if(s->cdata_sz > 0){
		fputs("cdata_sz=",stdout);
		mutoa(buf, s->cdata_sz);
		fputs(buf,stdout);
		fputs(" ",stdout);
	}
	astdump_print_type(s->t, should_print_isfunction);
	fputs(s->name, stdout);
}


static void astdump_printexpr(expr_node* e, uint64_t indentlevel){
	uint64_t c;
	uint64_t i;
	c = e->kind;
	fputs("\n",stdout);
	do_indent(indentlevel);
	if(c == EXPR_SIZEOF) fputs("sizeof",stdout);
	if(c == EXPR_INTLIT) fputs("intlit",stdout);
	if(c == EXPR_FLOATLIT) fputs("floatlit",stdout);
	if(c == EXPR_STRINGLIT) fputs("stringlit",stdout);
	if(c == EXPR_LSYM) { 
		fputs("local:",stdout); 
		fputs(e->symname,stdout);
	}
	if(c == EXPR_GSYM) {
		fputs("global:",stdout);
		fputs(e->symname,stdout);
	}
	if(c == EXPR_SYM) {
		fputs("sym_unknown:",stdout);
		fputs(e->symname,stdout);
	}
	if(c == EXPR_POST_INCR) fputs("post_incr",stdout);
	if(c == EXPR_POST_DECR) fputs("post_decr",stdout);
	if(c == EXPR_PRE_DECR) fputs("pre_decr",stdout);
	if(c == EXPR_PRE_INCR) fputs("pre_incr",stdout);
	if(c == EXPR_INDEX) fputs("[]",stdout);
	if(c == EXPR_MEMBER) {
		fputs(".",stdout);
		fputs(e->symname,stdout);
	}
	if(c == EXPR_METHOD) {
		fputs("method:",stdout);
		if(e->symname)
			fputs(e->symname,stdout);
		if(e->symname == NULL)
			if(e->method_name)
				fputs(e->method_name,stdout);
	}
	if(c == EXPR_CAST) {
		fputs("cast(",stdout);
		astdump_print_type(e->type_to_get_size_of,1);
		fputs(")",stdout);
	}
	if(c == EXPR_NEG) fputs("neg",stdout);
	if(c == EXPR_NOT) fputs("!",stdout);
	if(c == EXPR_COMPL) fputs("~",stdout);
	if(c == EXPR_MUL) fputs("*",stdout);
	if(c == EXPR_DIV) fputs("/",stdout);
	if(c == EXPR_MOD) fputs("%",stdout);
	if(c == EXPR_ADD) fputs("+",stdout);
	if(c == EXPR_SUB) fputs("-",stdout);
	if(c == EXPR_BITOR) fputs("|",stdout);
	if(c == EXPR_BITAND) fputs("&",stdout);
	if(c == EXPR_BITXOR) fputs("^",stdout);
	if(c == EXPR_LSH) fputs("<<",stdout);
	if(c == EXPR_RSH) fputs(">>",stdout);
	if(c == EXPR_LOGOR) fputs("||",stdout);
	if(c == EXPR_LOGAND) fputs("&&",stdout);
	if(c == EXPR_LT) fputs("<",stdout);
	if(c == EXPR_LTE) fputs("<=",stdout);
	if(c == EXPR_GT) fputs(">",stdout);
	if(c == EXPR_GTE) fputs(">=",stdout);
	if(c == EXPR_EQ) fputs("==",stdout);
	if(c == EXPR_NEQ) fputs("!=",stdout);
	if(c == EXPR_ASSIGN) fputs("=",stdout);
	if(c == EXPR_MOVE) fputs(":=",stdout);
	if(c == EXPR_FCALL) {
		fputs("fcall ",stdout);
		if(e->symname)
			fputs(e->symname,stdout);
	}
	if(c == EXPR_BUILTIN_CALL) {
		fputs("builtin_call ",stdout);
		if(e->symname)
			fputs(e->symname,stdout);
	}
	if(c == EXPR_CONSTEXPR_FLOAT) fputs("cexprf",stdout);
	if(c == EXPR_CONSTEXPR_INT) fputs("cexpri",stdout);
	//now dump the subexpressions.
	for(i = 0; i < MAX_FARGS; i++)
		if(e->subnodes[i] != NULL)
			astdump_printexpr(e->subnodes[i],indentlevel+4);
}

static void astdump_printscope(scope* s, uint64_t indentlevel);
static void astdump_printstmt(stmt* s, uint64_t indentlevel){
	fputs("\n",stdout);
	uint64_t i;
	char buf[80];
	fputs("\n",stdout);
	do_indent(indentlevel);
	if(s->kind == STMT_BAD) fputs("!!stmt_bad!!",stdout);
	if(s->kind == STMT_NOP) fputs("stmt_nop;",stdout);
	if(s->kind == STMT_EXPR) fputs("stmt_expr",stdout);
	if(s->kind == STMT_LABEL) {
		fputs("label:",stdout);
		fputs(s->referenced_label_name,stdout);
		fputs(";",stdout);
	}
	if(s->kind == STMT_GOTO) {
		fputs("goto ",stdout);
		fputs(s->referenced_label_name,stdout);
		if(s->referenced_loop == NULL){
			fputs("!!goto_referenced_stmt_is_invalid!!",stdout);
		}
		if(s->referenced_loop != NULL){
			if(s->referenced_loop->kind != STMT_LABEL){
				fputs("!!goto_referenced_stmt_is_not_label!!",stdout);
			}
			mutoa(buf, s->goto_scopediff);
			fputs("(scopediff=",stdout);
			fputs(buf,stdout);
			fputs(") ",stdout);
			mutoa(buf, s->goto_vardiff);
			fputs("(vardiff=",stdout);
			fputs(buf,stdout);
			fputs(") ",stdout);
			fputs(";",stdout);
		}
	}
	if(s->kind == STMT_WHILE){
		fputs("while",stdout);
	}
	if(s->kind == STMT_FOR){
		fputs("for",stdout);
	}
	if(s->kind == STMT_IF){
		fputs("if",stdout);
	}
	if(s->kind == STMT_ELIF){
		fputs("elif",stdout);
	}
	if(s->kind == STMT_ELSE){
		fputs("else",stdout);
	}
	if(s->kind == STMT_RETURN){
		fputs("return ",stdout);
		mutoa(buf, s->goto_scopediff);
		fputs("(scopediff=",stdout);
		fputs(buf,stdout);
		fputs(") ",stdout);
		mutoa(buf, s->goto_vardiff);
		fputs("(vardiff=",stdout);
		fputs(buf,stdout);
		fputs(") ",stdout);
		fputs(";",stdout);
	}
	if(s->kind == STMT_TAIL){
		fputs("tail ",stdout);
		fputs(s->referenced_label_name,stdout);
			mutoa(buf, s->goto_scopediff);
			fputs(" (scopediff=",stdout);
			fputs(buf,stdout);
			fputs(") ",stdout);
			mutoa(buf, s->goto_vardiff);
			fputs("(vardiff=",stdout);
			fputs(buf,stdout);
			fputs(") ",stdout);
			fputs(";",stdout);
	}
	if(s->kind == STMT_ASM){
		fputs("asm",stdout);
	}
	if(s->kind == STMT_CONTINUE){
		fputs("continue ",stdout);
		if(s->referenced_loop == NULL){
			fputs("!!null!!",stdout);
		}
		if(s->referenced_loop != NULL){
			if(s->referenced_loop->kind == STMT_FOR){
				fputs("for",stdout);
				goto valid_continue_loop_type;
			}
			if(s->referenced_loop->kind == STMT_WHILE){
				fputs("while",stdout);
				goto valid_continue_loop_type;
			}
			fputs("!!not_a_loop!!",stdout);
			valid_continue_loop_type:;
					mutoa(buf, s->goto_scopediff);
					fputs("(scopediff=",stdout);
					fputs(buf,stdout);
					fputs(") ",stdout);
					mutoa(buf, s->goto_vardiff);
					fputs("(vardiff=",stdout);
					fputs(buf,stdout);
					fputs(") ",stdout);
					fputs(";",stdout);
		}
	}
	if(s->kind == STMT_BREAK){
		fputs("break ",stdout);
		if(s->referenced_loop == NULL){
			fputs("null ",stdout);
		}
		if(s->referenced_loop != NULL){
			if(s->referenced_loop->kind == STMT_FOR){
				fputs("out of for",stdout);
				goto valid_break_loop_type;
			}
			if(s->referenced_loop->kind == STMT_WHILE){
				fputs("out of while",stdout);
				goto valid_break_loop_type;
			}
			fputs("!!not_a_loop!!",stdout);
			valid_break_loop_type:;
					mutoa(buf, s->goto_scopediff);
					fputs("(scopediff=",stdout);
					fputs(buf,stdout);
					fputs(") ",stdout);
					mutoa(buf, s->goto_vardiff);
					fputs("(vardiff=",stdout);
					fputs(buf,stdout);
					fputs(") ",stdout);
					fputs(";",stdout);
		}
	}
	if(s->kind == STMT_SWITCH){
		fputs("switch:",stdout);
		for(i = 0; i < s->switch_nlabels; i++){
			fputs(s->switch_label_list[i],stdout);
			{
				stmt* whereami_list;
				whereami_list = s->whereami->stmts;
				if(s->switch_label_indices[i] >= s->whereami->nstmts)
				{
					fputs("!!impossible_label_index!!",stdout);
				}
				if(s->switch_label_indices[i] < s->whereami->nstmts)
				{
					if(whereami_list[s->switch_label_indices[i]].kind != STMT_LABEL)
					{
						fputs("!!(switch index error)!!",stdout);
					}
					if(whereami_list[s->switch_label_indices[i]].referenced_label_name)
					if(
						!streq(
							whereami_list[s->switch_label_indices[i]].referenced_label_name,
							s->switch_label_list[i]
						)
					)
					{
						fputs("!!(switch index error)!!",stdout);
					}
				}
			}
			if(i != s->switch_nlabels - 1)
				fputs(",",stdout);
		}
	}
	if(s->kind > STMT_SWITCH){
		fputs("!!not_a_valid_statement!!",stdout);
	}
	//fputs("\n",stdout);
	for(i = 0; i < s->nexpressions && i < STMT_MAX_EXPRESSIONS; i++){
		mutoa(buf, i+1);
		if(s->expressions[i]){
			fputs("\n",stdout);
			do_indent(indentlevel+4);
			fputs("expr#",stdout);
			fputs(buf,stdout);
			astdump_printexpr(s->expressions[i],indentlevel+8);
		}
	}
	if(s->myscope != NULL)
		astdump_printscope(s->myscope, indentlevel+4);
	return;
}

static void astdump_printscope(scope* s, uint64_t indentlevel){
	uint64_t i;
	stmt* stmtlist;
	fputs("\n",stdout);
	do_indent(indentlevel);fputs("{\n",stdout);

	do_indent(indentlevel);fputs("~~locals:\n",stdout);

	for(i = 0; i < s->nsyms; i++)
		astdump_printsymbol(s->syms+i,indentlevel + 2, 1);
	fputs("\n",stdout);
	do_indent(indentlevel);fputs("~~statements:",stdout);

		stmtlist = s->stmts;
	for(i=0; i < s->nstmts; i++){
		astdump_printstmt(stmtlist + i,indentlevel+4);
	}
	fputs("\n",stdout);
	do_indent(indentlevel);fputs("}\n",stdout);
}


void astdump(){
	unsigned long i = 0;
	unsigned long j = 0;
	puts("~~GLOBAL SYMBOL TABLE~~");
	for(i = 0; i < nsymbols; i++)
		astdump_printsymbol(symbol_table + i, 1,0);
	puts("\n~~FUNCTION DUMP~~");
	for(i=0; i < nsymbols; i++)
	if(symbol_table[i].fbody)
	if(symbol_table[i].t.is_function)
	{

		fputs("\n",stdout);
		astdump_printsymbol(symbol_table+i,1,0);

		fputs("\nargs:",stdout);
		for(j = 0; j < symbol_table[i].nargs; j++){
			fputs("\n",stdout);
			do_indent(2);
			astdump_print_type(symbol_table[i].fargs[j][0],1);
		}
		astdump_printscope(symbol_table[i].fbody,4);
	}
}
