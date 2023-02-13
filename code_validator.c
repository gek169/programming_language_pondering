#include "stringutil.h"
#include "targspecifics.h"

#include "libmin.h"
#include "parser.h"
#include "data.h"
#include "metaprogramming.h"

/*from the VM.*/
uint64_t vm_get_offsetof(
	typedecl* the_struct, 
	char* membername
);

char** discovered_labels = NULL;
uint64_t n_discovered_labels = 0;
static stmt* curr_stmt = NULL;
/*target word and sword. Used for bitwise operations and if/switch/elif/while.*/
static uint64_t TARGET_WORD_BASE = BASE_U64;
static uint64_t TARGET_WORD_SIGNED_BASE = BASE_I64;
static uint64_t TARGET_MAX_FLOAT_TYPE = BASE_F64;
static uint64_t TARGET_DISABLE_FLOAT = 0;


void set_target_word(uint64_t val){
	TARGET_WORD_BASE = val;
	TARGET_WORD_SIGNED_BASE = val + 1;
}

void set_max_float_type(uint64_t val){
	if(val != BASE_F64 && val != BASE_F32){
		TARGET_MAX_FLOAT_TYPE = 0;
		TARGET_DISABLE_FLOAT = 1;
	} else {
		TARGET_MAX_FLOAT_TYPE = val;
		TARGET_DISABLE_FLOAT = 0;
	}
}

static inline int impl_streq_exists(){
	for(unsigned long i = 0; i < nsymbols; i++){
		if(streq("impl_streq", symbol_table[i].name)){
			if(symbol_table[i].t.is_function == 0) return 0;
			if(symbol_table[i].is_codegen > 0) return 0;
			if(symbol_table[i].is_pure == 0) return 0;
			if(symbol_table[i].t.basetype != TARGET_WORD_SIGNED_BASE) return 0;
			if(symbol_table[i].t.pointerlevel != 0) return 0;
			if(symbol_table[i].nargs != 2) return 0;
			if(symbol_table[i].fargs[0]->basetype != BASE_U8) return 0;
			if(symbol_table[i].fargs[0]->pointerlevel != 1) 	return 0;
			if(symbol_table[i].fargs[1]->basetype != BASE_U8) return 1;
			if(symbol_table[i].fargs[1]->pointerlevel != 1) 	return 1;
			return 1;
		}
	}
	return 0;
}

static void validator_exit_err(){
	char buf[80];
	if(curr_stmt){
		mutoa(buf, curr_stmt->linenum);
		if(curr_stmt->filename){
			puts("Here is the beginning (approximately...) of the statement which caused an error:");
			puts("File:");
			puts(curr_stmt->filename);
			puts("Line:");
			puts(buf);
			mutoa(buf, curr_stmt->colnum);
			puts("Column:");
			puts(buf);
			puts("~~~~\nNote that the line number and column number");
			puts("are where the validator invoked validator_exit_err.");
			puts("\n\n(the actual error may be slightly before,\nor on the previous line)");
			puts("I recommend looking near the location provided,\nnot just that exact spot!");
		}
		exit(1);
	}
	exit(1);
}


static int this_specific_scope_has_label(char* c, scope* s){
	uint64_t i;
	stmt* stmtlist = s->stmts;
	for(i = 0; i < s->nstmts; i++)
		if(stmtlist[i].kind == STMT_LABEL)
			if(streq(c, stmtlist[i].referenced_label_name)) /*this is the label we're looking for!*/
				return 1;
	return 0;
}

static uint64_t this_specific_scope_get_label_index(char* c, scope* s){
	uint64_t i;
	stmt* stmtlist = s->stmts;
	for(i = 0; i < s->nstmts; i++)
		if(stmtlist[i].kind == STMT_LABEL)
			if(streq(c, stmtlist[i].referenced_label_name)) /*this is the label we're looking for!*/
				return i;
	return 0;
}

static void checkswitch(stmt* sw){
	unsigned long i;
	require(sw->kind == STMT_SWITCH, "<VALIDATOR ERROR> checkswitch erroneously passed non-switch statement");
	/*switch never has a scopediff or vardiff because the labels must be in the same scope. Thus, no variables ever have to be popped.*/
	sw->goto_scopediff = 0;
	sw->goto_vardiff = 0;
	for(i = 0; i < sw->switch_nlabels; i++){
		if(!this_specific_scope_has_label(sw->switch_label_list[i], sw->whereami))
			{
				puts("Switch statement in function uses label not in its home scope.");
				puts("A switch statement is not allowed to jump out of its own scope, either up or down!");
				puts("Function name:");
				puts(symbol_table[active_function].name);
				puts("Label in particular:");
				puts(sw->switch_label_list[i]);
				validator_exit_err();
			}
		sw->switch_label_indices[i] = 
			this_specific_scope_get_label_index(sw->switch_label_list[i],sw->whereami);
		if(  ((stmt*)sw->whereami->stmts)[sw->switch_label_indices[i]].kind != STMT_LABEL){
			puts("Internal Validator Error");
			puts("this_specific_scope_get_label_index returned the index of a non-label.");
			validator_exit_err();
		}
		if(
			!streq(
				((stmt*)sw->whereami->stmts)
					[sw->switch_label_indices[i]]
						.referenced_label_name
				,
				sw->switch_label_list[i]
			)
		){
			puts("Internal Validator Error");
			puts("this_specific_scope_get_label_index returned the index of wrong label!");
			validator_exit_err();
		}
	}
	return;
}


static void check_label_declarations(scope* lvl){
	unsigned long i;
	unsigned long j;
	stmt* stmtlist = lvl->stmts;
	for(i = 0; i < lvl->nstmts; i++)
	{
		curr_stmt = stmtlist + i;
		if(stmtlist[i].myscope)
			check_label_declarations(stmtlist[i].myscope);
			/*
		if(stmtlist[i].myscope2)
			check_label_declarations(stmtlist[i].myscope2);
			*/
		if(stmtlist[i].kind == STMT_LABEL){
			for(j = 0; j < n_discovered_labels;j++){
				if(streq(stmtlist[i].referenced_label_name, discovered_labels[j]))
				{
					puts("Duplicate label in function:");
					puts(symbol_table[active_function].name);
					puts("Label is:");
					puts(stmtlist[i].referenced_label_name);
					validator_exit_err();
				}
			}
			/*add it to the list of discovered labels*/
			discovered_labels = realloc(discovered_labels, (++n_discovered_labels) * sizeof(char*));
			require(discovered_labels != NULL, "failed realloc");
			discovered_labels[n_discovered_labels-1] = stmtlist[i].referenced_label_name;
		}
		if(stmtlist[i].kind == STMT_SWITCH){
			/**/
			checkswitch(stmtlist+i);
		}
	}
	return;
}



static void assign_lsym_gsym(expr_node* ee){
	unsigned long i;
	unsigned long j;
	for(i = 0; i < MAX_FARGS; i++){
		if(ee->subnodes[i])
			assign_lsym_gsym(ee->subnodes[i]);
	}
	/*Now, do our assignment.*/
	if(ee->kind == EXPR_SYM)
		for(i = 0; i < nscopes; i++)
			for(j = 0; j < scopestack[i]->nsyms; j++)
				if(streq(ee->symname, scopestack[i]->syms[j].name)){
					ee->kind = EXPR_LSYM;
					ee->t = scopestack[i]->syms[j].t;
					return;
				}

	/*Mut search active_function's fargs...*/
	if(ee->kind == EXPR_SYM)
		for(i = 0; i < symbol_table[active_function].nargs; i++)
			if(
				streq(
					ee->symname, 
					symbol_table[active_function].fargs[i]->membername
				)
			){
				ee->kind = EXPR_LSYM;
				ee->t = *symbol_table[active_function].fargs[i];
				//ee->t.membername;
				return;
			}
	
	if(ee->kind == EXPR_SYM)
		for(i = 0; i < nsymbols; i++)
			if(streq(ee->symname, symbol_table[i].name)){
				/*Do some validation. It can't be a function...*/
				if(symbol_table[i].t.is_function)
				{
					puts("<INTERNAL ERROR> Uncaught usage of function name as identifier?");
					puts("Active Function:");
					puts(symbol_table[active_function].name);
					puts("Identifier:");
					if(ee->symname)
						puts(ee->symname);
					validator_exit_err();
				}
				if(symbol_table[active_function].is_pure > 0){
					puts("VALIDATOR ERROR!");
					puts("You may not use global variables in pure functions.");
					puts("This is the variable you are not allowed to access:");
					puts(ee->symname);
					puts("This is the function you tried to access it from:");
					puts(symbol_table[active_function].name);
					validator_exit_err();
				}
				ee->kind = EXPR_GSYM;
				ee->symid = i;
				//this function is impure because it uses global variables.
				symbol_table[active_function].is_impure_globals_or_asm = 1;
				symbol_table[active_function].is_impure = 1;
				return;
			}
	if(ee->kind == EXPR_SYM){
		puts("<VALIDATION ERROR> Unknown identifier...");
		puts("Active Function:");
		puts(symbol_table[active_function].name);
		puts("Identifier:");
		if(ee->symname)
			puts(ee->symname);
		validator_exit_err();
	}
	return;
}



static void throw_type_error(char* msg){
	puts("TYPING ERROR!");
	puts(msg);
	puts("Function:");
	puts(symbol_table[active_function].name);
	validator_exit_err();
}


static void throw_type_error_with_expression_enums(char* msg, unsigned a, unsigned b){

	unsigned c;
	puts("TYPING ERROR!");
	puts(msg);
	puts("Function:");
	puts(symbol_table[active_function].name);
	c = a;
	puts("a=");
	if(c == EXPR_SIZEOF) puts("EXPR_SIZEOF");
	if(c == EXPR_INTLIT) puts("EXPR_INTLIT");
	if(c == EXPR_FLOATLIT) puts("EXPR_FLOATLIT");
	if(c == EXPR_STRINGLIT) puts("EXPR_STRINGLIT");
	if(c == EXPR_LSYM) puts("EXPR_LSYM");
	if(c == EXPR_GSYM) puts("EXPR_GSYM");
	if(c == EXPR_SYM) puts("EXPR_SYM");
	if(c == EXPR_POST_INCR) puts("EXPR_POST_INCR");
	if(c == EXPR_POST_DECR) puts("EXPR_POST_DECR");
	if(c == EXPR_PRE_DECR) puts("EXPR_PRE_DECR");
	if(c == EXPR_PRE_INCR) puts("EXPR_PRE_INCR");
	if(c == EXPR_INDEX) puts("EXPR_INDEX");
	if(c == EXPR_MEMBER) puts("EXPR_MEMBER");
	if(c == EXPR_MEMBERPTR) puts("EXPR_MEMBERPTR");
	if(c == EXPR_METHOD) puts("EXPR_METHOD");
	if(c == EXPR_CAST) puts("EXPR_CAST");
	if(c == EXPR_NEG) puts("EXPR_NEG");
	if(c == EXPR_NOT) puts("EXPR_NOT");
	if(c == EXPR_COMPL) puts("EXPR_COMPL");
	if(c == EXPR_MUL) puts("EXPR_MUL");
	if(c == EXPR_DIV) puts("EXPR_DIV");
	if(c == EXPR_MOD) puts("EXPR_MOD");
	if(c == EXPR_ADD) puts("EXPR_ADD");
	if(c == EXPR_SUB) puts("EXPR_SUB");
	if(c == EXPR_BITOR) puts("EXPR_BITOR");
	if(c == EXPR_BITAND) puts("EXPR_BITAND");
	if(c == EXPR_BITXOR) puts("EXPR_BITXOR");
	if(c == EXPR_LSH) puts("EXPR_LSH");
	if(c == EXPR_RSH) puts("EXPR_RSH");
	if(c == EXPR_LOGOR) puts("EXPR_LOGOR");
	if(c == EXPR_LOGAND) puts("EXPR_LOGAND");
	if(c == EXPR_LT) puts("EXPR_LT");
	if(c == EXPR_LTE) puts("EXPR_LTE");
	if(c == EXPR_GT) puts("EXPR_GT");
	if(c == EXPR_GTE) puts("EXPR_GTE");
	if(c == EXPR_EQ) puts("EXPR_EQ");
	if(c == EXPR_STREQ) puts("EXPR_STREQ");
	if(c == EXPR_STRNEQ) puts("EXPR_STRNEQ");
	if(c == EXPR_NEQ) puts("EXPR_NEQ");
	if(c == EXPR_ASSIGN) puts("EXPR_ASSIGN");
	if(c == EXPR_MOVE) puts("EXPR_MOVE");
	if(c == EXPR_FCALL) puts("EXPR_FCALL");
	if(c == EXPR_GETFNPTR) puts("EXPR_GETFNPTR");
	if(c == EXPR_CALLFNPTR) puts("EXPR_CALLFNPTR");
	if(c == EXPR_BUILTIN_CALL) puts("EXPR_BUILTIN_CALL");
	if(c == EXPR_CONSTEXPR_FLOAT) puts("EXPR_CONSTEXPR_FLOAT");
	if(c == EXPR_CONSTEXPR_INT) puts("EXPR_CONSTEXPR_INT");

	if(b == EXPR_BAD) validator_exit_err();
	puts("b=");
	c = b;
	if(c == EXPR_SIZEOF) puts("EXPR_SIZEOF");
	if(c == EXPR_INTLIT) puts("EXPR_INTLIT");
	if(c == EXPR_FLOATLIT) puts("EXPR_FLOATLIT");
	if(c == EXPR_STRINGLIT) puts("EXPR_STRINGLIT");
	if(c == EXPR_LSYM) puts("EXPR_LSYM");
	if(c == EXPR_GSYM) puts("EXPR_GSYM");
	if(c == EXPR_SYM) puts("EXPR_SYM");
	if(c == EXPR_POST_INCR) puts("EXPR_POST_INCR");
	if(c == EXPR_POST_DECR) puts("EXPR_POST_DECR");
	if(c == EXPR_PRE_DECR) puts("EXPR_PRE_DECR");
	if(c == EXPR_PRE_INCR) puts("EXPR_PRE_INCR");
	if(c == EXPR_INDEX) puts("EXPR_INDEX");
	if(c == EXPR_MEMBER) puts("EXPR_MEMBER");
	if(c == EXPR_MEMBERPTR) puts("EXPR_MEMBERPTR");
	if(c == EXPR_METHOD) puts("EXPR_METHOD");
	if(c == EXPR_CAST) puts("EXPR_CAST");
	if(c == EXPR_NEG) puts("EXPR_NEG");
	if(c == EXPR_NOT) puts("EXPR_NOT");
	if(c == EXPR_COMPL) puts("EXPR_COMPL");
	if(c == EXPR_MUL) puts("EXPR_MUL");
	if(c == EXPR_DIV) puts("EXPR_DIV");
	if(c == EXPR_MOD) puts("EXPR_MOD");
	if(c == EXPR_ADD) puts("EXPR_ADD");
	if(c == EXPR_SUB) puts("EXPR_SUB");
	if(c == EXPR_BITOR) puts("EXPR_BITOR");
	if(c == EXPR_BITAND) puts("EXPR_BITAND");
	if(c == EXPR_BITXOR) puts("EXPR_BITXOR");
	if(c == EXPR_LSH) puts("EXPR_LSH");
	if(c == EXPR_RSH) puts("EXPR_RSH");
	if(c == EXPR_LOGOR) puts("EXPR_LOGOR");
	if(c == EXPR_LOGAND) puts("EXPR_LOGAND");
	if(c == EXPR_LT) puts("EXPR_LT");
	if(c == EXPR_LTE) puts("EXPR_LTE");
	if(c == EXPR_GT) puts("EXPR_GT");
	if(c == EXPR_GTE) puts("EXPR_GTE");
	if(c == EXPR_EQ) puts("EXPR_EQ");
	if(c == EXPR_STREQ) puts("EXPR_STREQ");
	if(c == EXPR_STRNEQ) puts("EXPR_STRNEQ");
	if(c == EXPR_NEQ) puts("EXPR_NEQ");
	if(c == EXPR_ASSIGN) puts("EXPR_ASSIGN");
	if(c == EXPR_MOVE) puts("EXPR_MOVE");
	if(c == EXPR_FCALL) puts("EXPR_FCALL");
	if(c == EXPR_GETFNPTR) puts("EXPR_GETFNPTR");
	if(c == EXPR_CALLFNPTR) puts("EXPR_CALLFNPTR");
	if(c == EXPR_BUILTIN_CALL) puts("EXPR_BUILTIN_CALL");
	if(c == EXPR_CONSTEXPR_FLOAT) puts("EXPR_CONSTEXPR_FLOAT");
	if(c == EXPR_CONSTEXPR_INT) puts("EXPR_CONSTEXPR_INT");

	
	validator_exit_err();
}

static void propagate_types(expr_node* ee){
	unsigned long i;
	unsigned long j;
	type t = {0};
	type t2 = {0};
	uint64_t WORD_BASE;
	uint64_t SIGNED_WORD_BASE;
	uint64_t FLOAT_BASE;
	if(symbol_table[active_function].is_codegen){
		WORD_BASE = BASE_U64;
		SIGNED_WORD_BASE = BASE_I64;
		FLOAT_BASE = BASE_F64;
	} else {
		WORD_BASE = TARGET_WORD_BASE;
		SIGNED_WORD_BASE = TARGET_WORD_SIGNED_BASE;
		FLOAT_BASE = TARGET_MAX_FLOAT_TYPE;
	}
	(void)WORD_BASE;
	
	for(i = 0; i < MAX_FARGS; i++){
		if(ee->subnodes[i])
			propagate_types(ee->subnodes[i]);
		if(!ee->subnodes[i]) continue; //in case we ever implement something that has null members...
	/*
		Do double-checking of the types in the subnodes.

		particularly, we may not have struct rvalues, or lvalue structs.

		must have pointer-to-struct or better.
	*/
		if(ee->subnodes[i]->t.basetype == BASE_STRUCT)
			if(ee->subnodes[i]->t.pointerlevel == 0)
			{
				throw_type_error_with_expression_enums(
					"expression node validated as having BASE_STRUCT without a pointer level..."
					,ee->subnodes[i]->kind,
					EXPR_BAD
				);
			}
		if(ee->subnodes[i]->t.arraylen > 0){
			throw_type_error_with_expression_enums(
				"Expression node validated as having an array length.",
				ee->subnodes[i]->kind,
				EXPR_BAD
			);
		}
		if(ee->subnodes[i]->t.is_function){
			throw_type_error_with_expression_enums(
				"Expression node validated as being a function. What?",
				ee->subnodes[i]->kind,
				EXPR_BAD
			);
		}
	}


/*
		TYPE GUARD

		prevent most illegal type operations.
*/

	if(ee->kind != EXPR_FCALL)/*Disallow void from this point onward.*/
	if(ee->kind != EXPR_BUILTIN_CALL)/*Disallow void from this point onward.*/
	if(ee->kind != EXPR_CALLFNPTR)/*Disallow void from this point onward.*/
	{
		if(ee->subnodes[0])
			if(ee->subnodes[0]->t.basetype == BASE_VOID)
				throw_type_error_with_expression_enums(
					"Math expressions may never have void in them. a=Parent,b=Child:",
					ee->kind,
					ee->subnodes[0]->kind
				);
		if(ee->subnodes[1])
			if(ee->subnodes[1]->t.basetype == BASE_VOID)
				throw_type_error_with_expression_enums(
					"Math expressions may never have void in them. a=Parent,b=Child:",
					ee->kind,
					ee->subnodes[0]->kind
				);
	}


	/*Forbid pointers for arithmetic purposes.*/
	if(ee->kind != EXPR_STREQ)
	if(ee->kind != EXPR_STRNEQ)
	if(ee->kind != EXPR_EQ)
	if(ee->kind != EXPR_NEQ)
	if(ee->kind != EXPR_ADD)
	if(ee->kind != EXPR_SUB)
	if(ee->kind != EXPR_FCALL)
	if(ee->kind != EXPR_BUILTIN_CALL)
	if(ee->kind != EXPR_CALLFNPTR)
	if(ee->kind != EXPR_PRE_INCR)
	if(ee->kind != EXPR_PRE_DECR)
	if(ee->kind != EXPR_POST_INCR)
	if(ee->kind != EXPR_POST_DECR)
	if(ee->kind != EXPR_ASSIGN) /*you can assign pointers...*/
	if(ee->kind != EXPR_MOVE) /*you can assign pointers...*/
	if(ee->kind != EXPR_CAST) /*You can cast pointers...*/
	if(ee->kind != EXPR_INDEX) /*You can index pointers...*/
	if(ee->kind != EXPR_METHOD) /*You invoke methods on or with pointers.*/
	if(ee->kind != EXPR_MEMBER) /*You can get members of structs which are pointed to.*/
	if(ee->kind != EXPR_MEMBERPTR) /*You can get pointers to members of structs which are pointed to.*/
	{
		if(ee->subnodes[0])
			if(ee->subnodes[0]->t.pointerlevel > 0)
				throw_type_error_with_expression_enums(
					"Pointer math is limited to addition, subtraction, assignment,moving, casting, equality comparison, and indexing.\n"
					"a=Parent,b=Child",
					ee->kind,
					ee->subnodes[0]->kind
				);
		if(ee->subnodes[1])
			if(ee->subnodes[1]->t.pointerlevel > 0)
				throw_type_error_with_expression_enums(
					"Pointer math is limited to addition, subtraction, assignment,moving, casting, equality comparison, and indexing.\n"
					"a=Parent,b=Child",
					ee->kind,
					ee->subnodes[1]->kind
				);
	}


	
	if(ee->kind == EXPR_INTLIT){
		//t.basetype = BASE_U64;
		t.basetype = WORD_BASE;
		t.is_lvalue = 0;
		t.pointerlevel = 0;
		ee->t = t;
		return;
	}
	if(ee->kind == EXPR_FLOATLIT){
		if(symbol_table[active_function].is_codegen == 0)
		if(TARGET_DISABLE_FLOAT)
		{
			puts("Validator error:");
			puts("Target has disabled floating point.");
			validator_exit_err();
		}
		//t.basetype = BASE_F64;
		t.basetype = FLOAT_BASE;
		t.is_lvalue = 0;
		t.pointerlevel = 0;
		ee->t = t;
		return;
	}
	if(ee->kind == EXPR_GSYM){
		t = symbol_table[ee->symid].t;
		if(t.arraylen){
			t.arraylen = 0;
			t.pointerlevel++;
			t.is_lvalue = 0;
		}
		if(t.pointerlevel == 0)
			if(t.basetype == BASE_STRUCT){
				t.pointerlevel = 1;
				t.is_lvalue = 0;
			}
		ee->t = t;
		return;
	}
	if(ee->kind == EXPR_LSYM){
		/*TODO*/
		t = ee->t;
		if(t.arraylen){
			t.arraylen = 0;
			t.pointerlevel++;
			t.is_lvalue = 0;
		}
		if(t.pointerlevel == 0)
			if(t.basetype == BASE_STRUCT){
				/*
				if(t.is_lvalue == 0)
				{
					throw_type_error_with_expression_enums(
						"Can't have Rvalue struct! a=me",
						ee->kind,
						EXPR_BAD
					);
				}
				*/
				t.pointerlevel = 1;
				t.is_lvalue = 0;
			}
		ee->t = t;
		ee->t.membername = NULL; /*If it was a function argument, we dont want the membername thing to be there.*/
		return;
	}
	if(ee->kind == EXPR_SIZEOF){
		//t.basetype = BASE_U64;
		t.basetype = WORD_BASE;
		t.is_lvalue = 0;
		t.pointerlevel = 0;
		ee->t = t;
		return;
	}
	if(ee->kind == EXPR_STRINGLIT){
		t.basetype = BASE_U8;
		t.pointerlevel = 1;
		t.is_lvalue = 0;
		ee->t = t;
		return;
	}
	if(ee->kind == EXPR_BUILTIN_CALL){
		uint64_t q = get_builtin_retval(ee->symname);
		t.is_lvalue = 0;
		if(symbol_table[active_function].is_pure > 0){
			puts("Validator error:");
			puts("Attempted to use a builtin call in a pure function.");
			validator_exit_err();
		}
		if(q == BUILTIN_PROTO_VOID){
			t.basetype = BASE_VOID;
			t.arraylen = 0;
			t.pointerlevel = 0;
			ee->t = t;
			return;
		}
		if(q == BUILTIN_PROTO_U8_PTR){
			t.basetype = BASE_U8;
			t.arraylen = 0;
			t.pointerlevel = 1;
			ee->t = t;
			return;
		}
		if(q == BUILTIN_PROTO_U8_PTR2){
			t.basetype = BASE_U8;
			t.arraylen = 0;
			t.pointerlevel = 2;
			ee->t = t;
			return;
		}
		if(q == BUILTIN_PROTO_U64_PTR){
			t.basetype = BASE_U64;
			t.arraylen = 0;
			t.pointerlevel = 1;
			ee->t = t;
			return;
		}
		if(q == BUILTIN_PROTO_U64){
			t.basetype = BASE_U64;
			t.arraylen = 0;
			t.pointerlevel = 0;
			ee->t = t;
			return;
		}
		if(q == BUILTIN_PROTO_I64){
			t.basetype = BASE_I64;
			t.arraylen = 0;
			t.pointerlevel = 0;
			ee->t = t;
			return;
		}
		if(q == BUILTIN_PROTO_DOUBLE){
			t.basetype = BASE_F64;
			t.arraylen = 0;
			t.pointerlevel = 0;
			ee->t = t;
			return;
		}
		if(q == BUILTIN_PROTO_I32){
			t.basetype = BASE_I32;
			t.arraylen = 0;
			t.pointerlevel = 0;
			ee->t = t;
			return;
		}
		puts("Internal Error: Unhandled __builtin function return value in type propagator.");
		puts("The unhandled one is:");
		puts(ee->symname);
		validator_exit_err();
	}
	if(ee->kind == EXPR_CONSTEXPR_FLOAT){
		//ee->t.basetype = BASE_F64;
		ee->t.basetype = FLOAT_BASE;
		t.is_lvalue = 0;
		t.pointerlevel = 0;
		return;
	}
	if(ee->kind == EXPR_CONSTEXPR_INT){
		//ee->t.basetype = BASE_I64;
		ee->t.basetype = SIGNED_WORD_BASE;
		t.is_lvalue = 0;
		t.pointerlevel = 0;
		return;
	}
	if(ee->kind == EXPR_POST_INCR || 
		ee->kind == EXPR_PRE_INCR ||
		ee->kind == EXPR_POST_DECR ||
		ee->kind == EXPR_PRE_DECR
	){
		t = ee->subnodes[0]->t;
		if(t.pointerlevel == 0){
			if(t.basetype == BASE_VOID){
				throw_type_error("Can't increment/decrement void!");
			}
			if(t.basetype == BASE_STRUCT){
				throw_type_error("Can't increment/decrement struct");
			}
			if(t.is_lvalue == 0){
				throw_type_error("Outside of a constant expression, it is invalid to increment or decrement a non-lvalue.");
			}
		}
		t.is_lvalue = 0; /*no longer an lvalue.*/
		ee->t = t;
		return;
	}	
	if(ee->kind == EXPR_INDEX){
		t = ee->subnodes[0]->t;
		if(t.pointerlevel == 0){
			throw_type_error("Can't index non-pointer!");
		}
		t.pointerlevel--;
		t.is_lvalue = 1; /*if it wasn't an lvalue before, it is now!*/
		if(t.pointerlevel == 0){
			if(t.basetype == BASE_STRUCT)
				throw_type_error("Can't deref pointer to struct.");
			if(t.basetype == BASE_VOID)
				throw_type_error("Can't deref pointer to void.");
		}
		ee->t = t;
		if(
			ee->subnodes[1]->t.pointerlevel > 0 ||
			ee->subnodes[1]->t.basetype == BASE_F32 ||
			ee->subnodes[1]->t.basetype == BASE_F64 ||
			ee->subnodes[1]->t.basetype == BASE_VOID ||
			ee->subnodes[1]->t.basetype == BASE_STRUCT
		){
			throw_type_error("Indexing requires an integer.");
		}
		return;
	}
	if(ee->kind == EXPR_MEMBER){
		//int found = 0;
		t = ee->subnodes[0]->t;
		if(t.basetype != BASE_STRUCT)
			throw_type_error_with_expression_enums(
				"Can't access member of non-struct! a=me",
				ee->kind,
				ee->subnodes[0]->kind
			);
		if(t.structid >= ntypedecls)
			throw_type_error("Internal error, invalid struct ID for EXPR_MEMBER");
		if(ee->symname == NULL)
			throw_type_error("Internal error, EXPR_MEMBER had null symname...");
		for(j = 0; j < type_table[t.structid].nmembers; j++){
			if(
				streq(
					type_table[t.structid].members[j].membername,
					ee->symname
				)
			){
			//	found = 1;
				ee->t = type_table[t.structid].members[j];
				//TODO: optimize this.
				ee->idata = vm_get_offsetof(type_table + t.structid, ee->symname);
				ee->t.is_lvalue = 1;
				//handle: struct member is array.
				if(ee->t.arraylen){
					ee->t.is_lvalue = 0;
					ee->t.arraylen = 0;
					ee->t.pointerlevel++;
				}
				//handle: struct member is also a struct, but not pointer to struct.
				//note that for an array of structs, pointerlevel was already set,
				//so we don't have to worry about that here.
				if(ee->t.basetype == BASE_STRUCT)
				if(ee->t.pointerlevel == 0){
					ee->t.is_lvalue = 0;
					ee->t.arraylen = 0;
					ee->t.pointerlevel = 1;
				}
				//handle:struct member is function
				if(ee->t.is_function){
					puts("Error: Struct member is function. How did that happen?");
					throw_type_error("Struct member is function.");
				}
				ee->t.membername = NULL; /*We don't want it!*/
				return;
			}
		}
		
		{
			puts("Struct:");
			puts(type_table[t.structid].name);
			puts("Does not have member:");
			puts(ee->symname);
			throw_type_error_with_expression_enums(
				"Struct lacking member. a=me",
				ee->kind,
				EXPR_BAD
			);
			validator_exit_err();
		}
		return;
	}

	if(ee->kind == EXPR_MEMBERPTR){
		//int found = 0;
		t = ee->subnodes[0]->t;
		if(t.basetype != BASE_STRUCT)
			throw_type_error_with_expression_enums(
				"Can't access member of non-struct! a=me",
				ee->kind,
				ee->subnodes[0]->kind
			);
		if(t.structid >= ntypedecls)
			throw_type_error("Internal error, invalid struct ID for EXPR_MEMBERPTR");
		if(ee->symname == NULL)
			throw_type_error("Internal error, EXPR_MEMBERPTR had null symname...");
		for(j = 0; j < type_table[t.structid].nmembers; j++){
			if(
				streq(
					type_table[t.structid].members[j].membername,
					ee->symname
				)
			){
			//	found = 1;
				ee->t = type_table[t.structid].members[j];
				ee->t.is_lvalue = 0;
				//TODO: optimize this.
				ee->idata = vm_get_offsetof(type_table + t.structid, ee->symname);
				//handle: struct member is array.
				if(ee->t.arraylen > 0){
					ee->t.arraylen = 0;
				}
				ee->t.pointerlevel++;
				//handle:struct member is function
				if(ee->t.is_function){
					puts("Error: Struct member is function. How did that happen?");
					throw_type_error("Struct member is function.");
				}
				ee->t.membername = NULL; /*We don't want it!*/
				return;
			}
		}
		{
			puts("Struct:");
			puts(type_table[t.structid].name);
			puts("Does not have member, therefore cannot retrieve pointer:");
			puts(ee->symname);
			throw_type_error_with_expression_enums(
				"Struct lacking member. a=me",
				ee->kind,
				EXPR_BAD
			);
			validator_exit_err();
		}
		return;
	}
	if(ee->kind == EXPR_METHOD){
		char* c;
		t = ee->subnodes[0]->t;
		if(t.basetype != BASE_STRUCT)
			throw_type_error("Can't call method on non-struct!");
		if(t.structid >= ntypedecls)
			throw_type_error("Internal error, invalid struct ID for EXPR_METHOD");
		if(ee->method_name == NULL)
			throw_type_error("Internal error, EXPR_METHOD had null methodname...");
		/*
			do name mangling.
		*/
		c = strcata("__method_", type_table[t.structid].name);
		c = strcataf1(c, "_____");
		c = strcataf1(c, ee->method_name);
		ee->symname = c;
		for(j = 0; j < nsymbols; j++)
			if(streq(c, symbol_table[j].name)){
				if(symbol_table[j].t.is_function == 0){
					puts("Weird things have been happening with the method table.");
					puts("It appears you've been using reserved (__) names...");
					puts("Don't do that!");
					puts("The method I was looking for is:");
					puts(c);
					puts("And I found a symbol by that name, but it is not a function.");
					throw_type_error("Method table messed up");
				}
				if(symbol_table[j].nargs == 0){
					puts("This method:");
					puts(c);
					puts("Takes no arguments? Huh?");
					throw_type_error("Method table messed up");
				}
				if(symbol_table[j].fargs[0]->basetype != BASE_STRUCT){
					puts("This method:");
					puts(c);
					puts("Does not take a struct pointer as its first argument. Huh?");
					throw_type_error("Method table messed up");
				}
				if(symbol_table[j].fargs[0]->structid != t.structid){
					puts("This method:");
					puts(c);
					puts("Was apparently somehow called, as a method, on a non-matching struct.");
					throw_type_error("Method shenanigans");
				}
				if(symbol_table[active_function].is_pure)
					if(symbol_table[j].is_pure == 0){
						puts("<VALIDATOR ERROR>");
						puts("You tried to invoke this method:");
						puts(ee->method_name);
						puts("On a struct of this type:");
						puts(type_table[t.structid].name);
						puts("But that method is not declared 'pure', and you are trying to invoke it in a pure function.");
						puts("The pure function's name is:");
						puts(symbol_table[active_function].name);
						throw_type_error("Purity check failure.");
					}
				if(!!symbol_table[active_function].is_codegen != !!symbol_table[j].is_codegen){
					puts("Validator Error");
					puts("You tried to invoke this method:");
					puts(ee->method_name);
					puts("On a struct of this type:");
					puts(type_table[t.structid].name);
					if(!!symbol_table[j].is_codegen)
						puts("But that method was declared codegen,");
					else
						puts("But that method was not declared codegen,");
					puts("And the active function:");
					puts(symbol_table[active_function].name);
					if(!!symbol_table[active_function].is_codegen)
						puts("is declared codegen.");
					else
						puts("is not declared codegen.");
					throw_type_error("Method codegen check failure.");
				}
				//count how many subnodes we have.
				for(i = 0; ee->subnodes[i] != NULL; i++);
				if(i != symbol_table[j].nargs){
					puts("This method:");
					puts(c);
					puts("Was called with the wrong number of arguments.");
					throw_type_error_with_expression_enums(
						"Method invoked with wrong number of arguments! a=me",
						ee->kind,
						EXPR_BAD
					);
				}
				t2 = symbol_table[j].t;
				t2.is_function = 0;
				t2.funcid = 0;
				t2.is_lvalue = 0;
				t2.membername = NULL;
				ee->t = t2;
				ee->symid = j;
				return;
			}
		puts("This method:");
		puts(c);
		puts("Does not exist!");
		validator_exit_err();
	}
	if(ee->kind == EXPR_FCALL){
		if(ee->symid >= nsymbols){
			throw_type_error("INTERNAL ERROR: EXPR_FCALL erroneously has bad symbol ID. This should have been resolved in parser.c");
		}
		if(symbol_table[active_function].is_pure > 0)
			if(symbol_table[ee->symid].is_pure == 0){
				puts("<VALIDATOR ERROR>");
				puts("You tried to invoke this function:");
				puts(symbol_table[ee->symid].name);
				puts("But that function is not declared 'pure', and you are trying to invoke it in a pure function.");
				puts("The pure function's name is:");
				puts(symbol_table[active_function].name);
				throw_type_error("Purity check failure.");
			}
		ee->t = symbol_table[ee->symid].t;
		ee->t.is_function = 0;
		ee->t.funcid = 0;
		ee->t.is_lvalue = 0; /*You can't assign to the output of a function*/
		return;
	}
	if(ee->kind == EXPR_CALLFNPTR){
		if(ee->symid >= nsymbols){
			throw_type_error("INTERNAL ERROR: EXPR_CALLFNPTR erroneously has bad symbol ID. This should have been resolved in parser.c");
		}
		if(symbol_table[active_function].is_pure > 0){
			puts("<VALIDATOR ERROR>");
			puts("You tried to invoke a function pointer in a pure function. That is not allowed!");
			validator_exit_err();
		}
		ee->t = symbol_table[ee->symid].t;
		ee->t.is_function = 0;
		ee->t.funcid = 0;
		ee->t.is_lvalue = 0; /*You can't assign to the output of a function*/
		return;
	}
	/*always yields a char* */
	if(ee->kind == EXPR_GETFNPTR){
		ee->t = type_init();
		ee->t.basetype = BASE_U8;
		ee->t.pointerlevel = 1;
		ee->t.is_lvalue = 0;
		return;
	}
	if(ee->kind == EXPR_CAST){
		t = ee->type_to_get_size_of;
		t2 = ee->subnodes[0]->t;
		if(t.basetype == BASE_STRUCT)
			if(t.pointerlevel == 0)
				throw_type_error_with_expression_enums(
					"Cannot cast to struct. a=Parent, b=Child",
					ee->kind,
					ee->subnodes[0]->kind
				);
		if(t.pointerlevel == 0)
			if(t.basetype == BASE_VOID)
				throw_type_error("Cannot cast to void.");
		/*Allow arbitrary pointer-to-pointer casts.*/
		if(t.pointerlevel > 0)
		if(t2.pointerlevel > 0)
		{
			ee->t = t;
			return;
		}
		if(t.pointerlevel > 0){
			if(t2.basetype == BASE_F64 ||
				t2.basetype == BASE_F32
			)
			throw_type_error("You cannot cast floating point types to a pointer.");
		}
		if(t2.pointerlevel > 0){
			if(t.basetype == BASE_F64 ||
				t.basetype == BASE_F32
			)
			throw_type_error("You cannot cast pointer types to a floating point number.");
		}
		if(t2.pointerlevel == 0)
			if(t2.basetype == BASE_VOID)
				throw_type_error("You Cannot cast void to anything, no matter how hard you try!");
		ee->t = t;
		ee->t.is_lvalue = 0; /*You can't assign to the output of an assignment statement.*/
		return;
	}
	
	if(ee->kind == EXPR_STREQ){
		t = ee->subnodes[0]->t;
		t2 = ee->subnodes[1]->t;
		if(t.pointerlevel != 1 ||
			t2.pointerlevel != 1 ||
			t.basetype != BASE_U8 ||
			t2.basetype != BASE_U8){
				throw_type_error("EXPR_STREQ requires two char pointers!");
			}
		if(symbol_table[active_function].is_codegen == 0)
		{
			if(!impl_streq_exists()){
				puts("Validator error:");
				puts("usage of streq and strneq require this function to be defined with this exact prototype:");
				puts("fn pure impl_streq(u8* a, u8* b)->i64;");
				validator_exit_err();
			}
		}
		ee->t = type_init();
		//ee->t.basetype = BASE_I64;
		ee->t.basetype = SIGNED_WORD_BASE;
		ee->t.is_lvalue = 0;
		return;
	}
	if(ee->kind == EXPR_STRNEQ){
		t = ee->subnodes[0]->t;
		t2 = ee->subnodes[1]->t;
		if(t.pointerlevel != 1 ||
			t2.pointerlevel != 1 ||
			t.basetype != BASE_U8 ||
			t2.basetype != BASE_U8){
				throw_type_error("EXPR_STRNEQ requires two char pointers!");
			}
		if(symbol_table[active_function].is_codegen == 0)
		{
			if(!impl_streq_exists()){
				puts("Validator error:");
				puts("usage of streq and strneq require this function to be defined with this exact prototype:");
				puts("fn pure impl_streq(u8* a, u8* b)->i64");
				puts("Note this declaration is allowed to be public or static.");
				puts("You may also change the names of the variables.");
				validator_exit_err();
			}
		}
		ee->t = type_init();
		//ee->t.basetype = BASE_I64;
		ee->t.basetype = SIGNED_WORD_BASE;
		ee->t.is_lvalue = 0;
		return;
	}

	if(ee->kind == EXPR_EQ ||
		ee->kind == EXPR_NEQ ||
		ee->kind == EXPR_LT ||
		ee->kind == EXPR_LTE ||
		ee->kind == EXPR_GT ||
		ee->kind == EXPR_GTE
	){
			t = ee->subnodes[0]->t;
			t2 = ee->subnodes[1]->t;
		if(
			(t.pointerlevel != t2.pointerlevel)
		){
			throw_type_error_with_expression_enums("Comparison between incompatible types. Operands:",
				ee->subnodes[0]->kind,
				ee->subnodes[1]->kind
			);
		}

		ee->t = type_init();
		//ee->t.basetype = BASE_I64;
		ee->t.basetype = SIGNED_WORD_BASE;
		ee->t.is_lvalue = 0;
		return;
	}

	if(ee->kind == EXPR_ASSIGN){
		if(
			(ee->subnodes[0]->t.pointerlevel != ee->subnodes[1]->t.pointerlevel)
		){
			throw_type_error_with_expression_enums(
				"Assignment between incompatible types (pointerlevel) Operands:",
				ee->subnodes[0]->kind,
				ee->subnodes[1]->kind
			);
		}
		if(ee->subnodes[0]->t.pointerlevel > 0){
			if(
				ee->subnodes[0]->t.basetype != 
				ee->subnodes[1]->t.basetype
			)
				throw_type_error_with_expression_enums(
					"Assignment between incompatible pointer types (basetype) Operands:",
					ee->subnodes[0]->kind,
					ee->subnodes[1]->kind
				);
		}
		if(ee->subnodes[0]->t.is_lvalue == 0){
			throw_type_error_with_expression_enums("Cannot assign to non lvalue. Operands:",
				ee->subnodes[0]->kind,
				ee->subnodes[1]->kind
			);
		}
		ee->t = type_init();
		ee->t = ee->subnodes[0]->t; /*it assigns, and then gets that value.*/
		ee->t.is_lvalue = 0; /*You can't assign to the output of an assignment statement.*/
		return;
	}
	if(ee->kind == EXPR_MOVE){
		if(
			(ee->subnodes[0]->t.pointerlevel != ee->subnodes[1]->t.pointerlevel)
		){
			throw_type_error_with_expression_enums(
				"Move between incompatible types (pointerlevel) Operands:",
				ee->subnodes[0]->kind,
				ee->subnodes[1]->kind
			);
		}
		if(
			ee->subnodes[0]->t.basetype != 
			ee->subnodes[1]->t.basetype
		)
			throw_type_error_with_expression_enums(
				"Move between incompatible pointer types (basetype) Operands:",
				ee->subnodes[0]->kind,
				ee->subnodes[1]->kind
			);
		ee->t = ee->subnodes[0]->t;
		ee->t.is_lvalue = 0;
		return;
	}
	
	if(ee->kind == EXPR_NEG){
		t = ee->subnodes[0]->t;
		if(t.pointerlevel > 0)
			throw_type_error("Cannot negate pointer.");
		if(t.basetype == BASE_U8) t.basetype = BASE_I8;
		if(t.basetype == BASE_U16) t.basetype = BASE_I16;
		if(t.basetype == BASE_U32) t.basetype = BASE_I32;
		if(t.basetype == BASE_U64) t.basetype = BASE_I64;
		ee->t = t;
		ee->t.is_lvalue = 0;
		return;
	}
	if(ee->kind == EXPR_COMPL || 
		ee->kind == EXPR_NOT ||
		ee->kind == EXPR_LOGOR ||
		ee->kind == EXPR_LOGAND ||
		ee->kind == EXPR_BITOR ||
		ee->kind == EXPR_BITAND ||
		ee->kind == EXPR_BITXOR ||
		ee->kind == EXPR_LSH ||
		ee->kind == EXPR_RSH
	){
		t = ee->subnodes[0]->t;
		//t.basetype = BASE_I64;
		t.basetype = SIGNED_WORD_BASE;
		ee->t = t;
		ee->t.is_lvalue = 0;
		return;
	}
	if(ee->kind == EXPR_MUL ||
		ee->kind == EXPR_DIV
	){
		t = ee->subnodes[0]->t;
		t2 = ee->subnodes[1]->t;
		
		if(t.pointerlevel > 0 ||
			t2.pointerlevel > 0
		)throw_type_error_with_expression_enums(
				"Cannot do multiplication or division on a pointer.",
				ee->subnodes[0]->kind,
				ee->subnodes[1]->kind
		);

		
		ee->t.basetype = type_promote(t.basetype, t2.basetype);
		ee->t.is_lvalue = 0;
		return;
	}
	if(ee->kind == EXPR_MOD){
		t = ee->subnodes[0]->t;
		t2 = ee->subnodes[1]->t;
		if(t.pointerlevel > 0 ||
			t2.pointerlevel > 0)
			throw_type_error_with_expression_enums(
				"Cannot do multiplication, division, or modulo on a pointer.",
				ee->subnodes[0]->kind,
				ee->subnodes[1]->kind
			);
		if(
			t.basetype == BASE_F32 ||
			t.basetype == BASE_F64 ||
			t2.basetype == BASE_F32 ||
			t2.basetype == BASE_F64
		)
			throw_type_error_with_expression_enums(
				"You can't modulo floats. Not allowed!",
				ee->subnodes[0]->kind,
				ee->subnodes[1]->kind
			);
		ee->t.basetype = type_promote(t.basetype, t2.basetype);
		ee->t.is_lvalue = 0;
		return;
	}
	if(ee->kind == EXPR_ADD){
		ee->t = type_init();
		t = ee->subnodes[0]->t;
		t2 = ee->subnodes[1]->t;
		if(
			t.pointerlevel > 0 &&
			t2.pointerlevel > 0
		)
		throw_type_error_with_expression_enums(
			"Cannot add two pointers.",
			ee->subnodes[0]->kind,
			ee->subnodes[1]->kind
		);

		if(t.pointerlevel > 0 && t2.pointerlevel == 0){
			if(t2.basetype == BASE_F32 || t2.basetype == BASE_F64)
				throw_type_error("Cannot add float to pointer.");
			ee->t = t;
			ee->t.is_lvalue = 0;
			return;
		}
		if(t2.pointerlevel > 0 && t.pointerlevel == 0){
			if(t.basetype == BASE_F32 || t.basetype == BASE_F64)
				throw_type_error("Cannot add float to pointer.");
			ee->t = t2;
			ee->t.is_lvalue = 0;
			return;
		}
		ee->t.basetype = type_promote(t.basetype, t2.basetype);
		ee->t.is_lvalue = 0;
		return;
	}

	
	if(ee->kind == EXPR_SUB){
		ee->t = type_init();
		t = ee->subnodes[0]->t;
		t2 = ee->subnodes[1]->t;
		if(t2.pointerlevel > 0)
			throw_type_error_with_expression_enums(
				"Cannot subtract with second value being pointer. Operands:",
				ee->subnodes[0]->kind,
				ee->subnodes[1]->kind
			);
		/*Neither are pointers? Do type promotion.*/
		if(t.pointerlevel == 0){
			ee->t.basetype = type_promote(t.basetype, t2.basetype);
			return;
		}
		ee->t = t;
		ee->t.is_lvalue = 0;
		return;
	}

	throw_type_error_with_expression_enums(
		"Internal error, ee->kind type is unhandled or fell through.",
		ee->kind,
		EXPR_BAD
	);
	
	/**/
}



static void throw_if_types_incompatible(
	type a,
	type b,
	char* msg
){
	if(
		(a.pointerlevel != b.pointerlevel)
	){
		puts(msg);
		puts("incompatible types, due to pointerlevel");
		validator_exit_err();
	}
	if(a.pointerlevel > 0){
		if(
			a.basetype != 
			b.basetype
		)
		{
			puts(msg);
			puts("incompatible basetypes");
			validator_exit_err();
		}
	}
}


static void validate_function_argument_passing(expr_node* ee){
	unsigned long i;
	uint64_t nargs;
	uint64_t got_builtin_arg1_type;
	uint64_t got_builtin_arg2_type;
	uint64_t got_builtin_arg3_type;
	type t_target = {0};
	for(i = 0; i < MAX_FARGS; i++){
		if(ee->subnodes[i])
			validate_function_argument_passing(ee->subnodes[i]);
	}


	/*
		TODO
	*/
	if(ee->kind == EXPR_BUILTIN_CALL){
		nargs = get_builtin_nargs(ee->symname);
		t_target = type_init();
		/*The hardest one!!*/
		if(nargs == 0) return; /*EZ!*/
	 	got_builtin_arg1_type = get_builtin_arg1_type(ee->symname);
	 	if(nargs > 1)
			got_builtin_arg2_type = get_builtin_arg2_type(ee->symname);
		if(nargs > 2)
			got_builtin_arg3_type = get_builtin_arg3_type(ee->symname);
		/*Check argument 1.*/
		if(got_builtin_arg1_type == BUILTIN_PROTO_VOID){
			puts("Internal error: Argument 1 is BUILTIN_PROTO_VOID for EXPR_BUILITIN_CALL. Update metaprogramming.");
			throw_type_error("Internal error: BUILTIN_PROTO_VOID for EXPR_BUILTIN_CALL");
		}
		if(got_builtin_arg1_type == BUILTIN_PROTO_U8_PTR){
			t_target.basetype = BASE_U8;
			t_target.pointerlevel = 1;
		}
		if(got_builtin_arg1_type == BUILTIN_PROTO_U8_PTR2){
			t_target.basetype = BASE_U8;
			t_target.pointerlevel = 2;
		}
		if(got_builtin_arg1_type == BUILTIN_PROTO_U8_PTR2){
			t_target.basetype = BASE_U8;
			t_target.pointerlevel = 2;
		}
		if(got_builtin_arg1_type == BUILTIN_PROTO_U64_PTR){
			t_target.basetype = BASE_U64;
			t_target.pointerlevel = 1;
		}		
		if(got_builtin_arg1_type == BUILTIN_PROTO_U64){
			t_target.basetype = BASE_U64;
			t_target.pointerlevel = 0;
		}
		if(got_builtin_arg1_type == BUILTIN_PROTO_I64){
			t_target.basetype = BASE_I64;
			t_target.pointerlevel = 0;
		}
		if(got_builtin_arg1_type == BUILTIN_PROTO_DOUBLE){
			t_target.basetype = BASE_F64;
			t_target.pointerlevel = 0;
		}
		if(got_builtin_arg1_type == BUILTIN_PROTO_I32){
			t_target.basetype = BASE_I32;
			t_target.pointerlevel = 0;
		}
		throw_if_types_incompatible(
			t_target, 
			ee->subnodes[0]->t, 
			"First argument passed to builtin function is wrong!"
		);
		if(nargs < 2) return;
		t_target = type_init();
		if(got_builtin_arg2_type == BUILTIN_PROTO_VOID){
			puts("Internal error: Argument 2 is BUILTIN_PROTO_VOID for EXPR_BUILITIN_CALL. Update metaprogramming.");
			throw_type_error("Internal error: BUILTIN_PROTO_VOID for EXPR_BUILTIN_CALL");
		}
		if(got_builtin_arg2_type == BUILTIN_PROTO_U8_PTR){
			t_target.basetype = BASE_U8;
			t_target.pointerlevel = 1;
		}
		if(got_builtin_arg2_type == BUILTIN_PROTO_U8_PTR2){
			t_target.basetype = BASE_U8;
			t_target.pointerlevel = 2;
		}
		if(got_builtin_arg2_type == BUILTIN_PROTO_U8_PTR2){
			t_target.basetype = BASE_U8;
			t_target.pointerlevel = 2;
		}
		if(got_builtin_arg2_type == BUILTIN_PROTO_U64_PTR){
			t_target.basetype = BASE_U64;
			t_target.pointerlevel = 1;
		}		
		if(got_builtin_arg2_type == BUILTIN_PROTO_U64){
			t_target.basetype = BASE_U64;
			t_target.pointerlevel = 0;
		}
		if(got_builtin_arg2_type == BUILTIN_PROTO_I64){
			t_target.basetype = BASE_I64;
			t_target.pointerlevel = 0;
		}
		if(got_builtin_arg2_type == BUILTIN_PROTO_DOUBLE){
			t_target.basetype = BASE_F64;
			t_target.pointerlevel = 0;
		}
		if(got_builtin_arg2_type == BUILTIN_PROTO_I32){
			t_target.basetype = BASE_I32;
			t_target.pointerlevel = 0;
		}
		throw_if_types_incompatible(
			t_target, 
			ee->subnodes[1]->t, 
			"Second argument passed to builtin function is wrong!"
		);
		if(nargs < 3) return;
		t_target = type_init();
		if(got_builtin_arg3_type == BUILTIN_PROTO_VOID){
			puts("Internal error: Argument 3 is BUILTIN_PROTO_VOID for EXPR_BUILITIN_CALL. Update metaprogramming.");
			throw_type_error("Internal error: BUILTIN_PROTO_VOID for EXPR_BUILTIN_CALL");
		}
		if(got_builtin_arg3_type == BUILTIN_PROTO_U8_PTR){
			t_target.basetype = BASE_U8;
			t_target.pointerlevel = 1;
		}
		if(got_builtin_arg3_type == BUILTIN_PROTO_U8_PTR2){
			t_target.basetype = BASE_U8;
			t_target.pointerlevel = 2;
		}
		if(got_builtin_arg3_type == BUILTIN_PROTO_U8_PTR2){
			t_target.basetype = BASE_U8;
			t_target.pointerlevel = 2;
		}
		if(got_builtin_arg3_type == BUILTIN_PROTO_U64_PTR){
			t_target.basetype = BASE_U64;
			t_target.pointerlevel = 1;
		}		
		if(got_builtin_arg3_type == BUILTIN_PROTO_U64){
			t_target.basetype = BASE_U64;
			t_target.pointerlevel = 0;
		}
		if(got_builtin_arg3_type == BUILTIN_PROTO_I64){
			t_target.basetype = BASE_I64;
			t_target.pointerlevel = 0;
		}
		if(got_builtin_arg3_type == BUILTIN_PROTO_DOUBLE){
			t_target.basetype = BASE_F64;
			t_target.pointerlevel = 0;
		}
		if(got_builtin_arg3_type == BUILTIN_PROTO_I32){
			t_target.basetype = BASE_I32;
			t_target.pointerlevel = 0;
		}
		throw_if_types_incompatible(
			t_target, 
			ee->subnodes[2]->t, 
			"Third argument passed to builtin function is wrong!"
		);
	}

	if(ee->kind == EXPR_FCALL || ee->kind == EXPR_METHOD || ee->kind == EXPR_CALLFNPTR){
		char buf_err[1024];
		strcpy(buf_err, "Function Argument Number ");

		
		nargs = symbol_table[ee->symid].nargs;
		for(i = 0; i < nargs; i++){
			if(ee->kind == EXPR_FCALL)
				strcpy(buf_err, "fn arg ");
			if(ee->kind == EXPR_METHOD)
				strcpy(buf_err, "method arg ");
			if(ee->kind == EXPR_CALLFNPTR)
				strcpy(buf_err, "func pointer arg ");
			mutoa(buf_err + strlen(buf_err), i+1);
			strcat(buf_err, " has the wrong type.\n");
			if(ee->kind == EXPR_METHOD)
				strcat(
					buf_err,
					"Note that argument number includes 'this' as the invisible first argument."
				);
			if(ee->kind != EXPR_CALLFNPTR){
				throw_if_types_incompatible(
					symbol_table[ee->symid].fargs[i][0], 
					ee->subnodes[i]->t,
					buf_err
				);
			} else{
				throw_if_types_incompatible(
					symbol_table[ee->symid].fargs[i][0], 
					ee->subnodes[i+1]->t,
					buf_err
				);
			}
		}
		return;
	}

}


static void validate_codegen_safety(expr_node* ee){
	unsigned long i;
	for(i = 0; i < MAX_FARGS; i++){
		if(ee->subnodes[i])
			validate_codegen_safety(ee->subnodes[i]);
	}

	/*
		codegen functions may not access non-codegen variables,
		or call non-codegen code.
		Enforce that!
	*/
	if(
		ee->kind == EXPR_GSYM||
		ee->kind == EXPR_FCALL || 
		ee->kind == EXPR_METHOD ||
		ee->kind == EXPR_CALLFNPTR ||
		ee->kind == EXPR_GETFNPTR
	)
		if(
			symbol_table[active_function].is_codegen !=
			symbol_table[ee->symid].is_codegen
		){
			puts("Codegen safety check failed!");
			puts("This function:");
			puts(symbol_table[active_function].name);
			puts("May not at any time use this symbol:");
			puts(symbol_table[ee->symid].name);
			puts("Because one of them is declared 'codegen'");
			puts("and the other is not!");
			validator_exit_err();
		}
	if(symbol_table[active_function].is_codegen == 0)
		if(ee->kind == EXPR_BUILTIN_CALL){
			puts("Codegen safety check failed!");
			puts("This function:");
			puts(symbol_table[active_function].name);
			puts("May not invoke builtin functions. Including this one:");
			puts(ee->symname);
			puts("Because it is not declared 'codegen'");
			validator_exit_err();
		}
}

static void insert_implied_type_conversion(expr_node** e_ptr, type t){
	expr_node cast = {0};
	expr_node* allocated = NULL;

	t.is_lvalue = 0; /*No longer an lvalue after conversion.*/
	cast.kind = EXPR_CAST;
	cast.type_to_get_size_of = t;
	cast.t = t;
	cast.subnodes[0] = *e_ptr;
	cast.is_implied = 1;


	if(t.pointerlevel != e_ptr[0][0].t.pointerlevel){
		throw_type_error("Cannot insert implied cast where pointerlevel is not equal!");
	}
	if(t.pointerlevel > 0){
		if(t.basetype != e_ptr[0][0].t.basetype){
			throw_type_error("Cannot insert implied cast between invalid pointers!");
		}
		if(t.basetype == BASE_STRUCT)
		if(e_ptr[0][0].t.structid != t.structid)
			throw_type_error("Cannot insert implied cast between invalid pointers (to structs)!");
	}
	
	if(t.basetype == BASE_STRUCT)
	if(e_ptr[0][0].t.basetype != BASE_STRUCT)
		throw_type_error("Cannot insert implied cast from non struct to struct!");

	if(e_ptr[0][0].t.basetype == BASE_STRUCT)
	if(t.basetype != BASE_STRUCT)
		throw_type_error("Cannot insert implied cast from struct to non-struct!");

	if(t.basetype == BASE_STRUCT)
	if(e_ptr[0][0].t.structid != t.structid)
		throw_type_error("Cannot insert implied cast between invalid structs!");
		


		/*Identical basetype, pointerlevel, and is_lvalue?*/
	if(t.is_lvalue == e_ptr[0][0].t.is_lvalue)
		if(t.basetype == e_ptr[0][0].t.basetype)
			if(t.pointerlevel == e_ptr[0][0].t.pointerlevel)
				return;
	/*
		A type conversion is necessary.
		Even if it means that we're just converting a pointer to an integer.
	*/
	allocated = calloc(sizeof(expr_node),1);
	allocated[0] = cast;
	e_ptr[0] = allocated;
}

static void propagate_implied_type_conversions(expr_node* ee){
	unsigned long i;
	uint64_t nargs;
	type t_target = {0};
	uint64_t WORD_BASE;
	uint64_t SIGNED_WORD_BASE;
	uint64_t FLOAT_BASE;
	if(symbol_table[active_function].is_codegen){
		WORD_BASE = BASE_U64;
		SIGNED_WORD_BASE = BASE_I64;
		FLOAT_BASE = BASE_F64;
	} else {
		WORD_BASE = TARGET_WORD_BASE;
		SIGNED_WORD_BASE = TARGET_WORD_SIGNED_BASE;
		FLOAT_BASE = TARGET_MAX_FLOAT_TYPE;
	}
	(void)WORD_BASE;


	for(i = 0; i < MAX_FARGS; i++){
		if(ee->subnodes[i])
			propagate_implied_type_conversions(ee->subnodes[i]);
	}


	/*The ones we don't do anything for.*/
	if(
		ee->kind == EXPR_SIZEOF ||
		ee->kind == EXPR_INTLIT ||
		ee->kind == EXPR_FLOATLIT ||
		ee->kind == EXPR_STRINGLIT ||
		ee->kind == EXPR_LSYM ||
		ee->kind == EXPR_GSYM ||
		ee->kind == EXPR_POST_INCR ||
		ee->kind == EXPR_PRE_INCR ||
		ee->kind == EXPR_POST_DECR ||
		ee->kind == EXPR_PRE_DECR ||
		//ee->kind == EXPR_MEMBER ||
		ee->kind == EXPR_CAST ||
		ee->kind == EXPR_CONSTEXPR_FLOAT ||
		ee->kind == EXPR_CONSTEXPR_INT
	) return;

	if(ee->kind == EXPR_MEMBER){
		t_target = ee->subnodes[0]->t;
		t_target.is_lvalue = 0; //must be: not an lvalue.
		insert_implied_type_conversion(
			ee->subnodes+0, 
			t_target
		);
		return;
	}
	if(ee->kind == EXPR_MEMBERPTR){
		t_target = ee->subnodes[0]->t;
		t_target.is_lvalue = 0; //must be: not an lvalue.
		insert_implied_type_conversion(
			ee->subnodes+0, 
			t_target
		);
		return;
	}
	
	if(ee->kind == EXPR_INDEX){
		//t_target.basetype = BASE_I64;
		t_target.basetype = SIGNED_WORD_BASE;
		t_target.is_lvalue = 0;
		insert_implied_type_conversion(
			ee->subnodes+1, 
			t_target
		);
		t_target = ee->subnodes[0]->t;
		t_target.is_lvalue = 0;
		insert_implied_type_conversion(
			ee->subnodes+0, 
			t_target
		);
		return;
	}
	if(ee->kind == EXPR_NEG){
		t_target = ee->t;
		t_target.is_lvalue = 0;
		if(t_target.pointerlevel > 0)
			throw_type_error("Cannot negate pointer.");
		if(t_target.basetype == BASE_U8 ||
			t_target.basetype == BASE_U16 ||
			t_target.basetype == BASE_U32 ||
			t_target.basetype == BASE_U64
		) t_target.basetype++;

		if(t_target.basetype < BASE_U8 && t_target.basetype > BASE_F64)
			throw_type_error("Cannot negate non-numeric type.");
		insert_implied_type_conversion(
			ee->subnodes + 0,
			t_target
		);
		return;
	}

	if(
		ee->kind == EXPR_COMPL ||
		ee->kind == EXPR_NOT
	){
		t_target = type_init();
		//t_target.basetype = BASE_I64;

		t_target.basetype = SIGNED_WORD_BASE;
		insert_implied_type_conversion(
			ee->subnodes + 0,
			t_target
		);
		return;
	}

	if(
		ee->kind == EXPR_BITOR ||
		ee->kind == EXPR_BITAND ||
		ee->kind == EXPR_BITXOR ||
		ee->kind == EXPR_LOGOR ||
		ee->kind == EXPR_LOGAND ||
		ee->kind == EXPR_LSH ||
		ee->kind == EXPR_RSH
	){
		//t_target.basetype = BASE_I64;
		t_target.basetype = SIGNED_WORD_BASE;
		insert_implied_type_conversion(
			ee->subnodes + 0,
			t_target
		);
		insert_implied_type_conversion(
			ee->subnodes + 1,
			t_target
		);
		return;
	}

	/*most of the binops...*/
	if(ee->kind == EXPR_MUL ||
		ee->kind == EXPR_DIV ||
		ee->kind == EXPR_MOD ||
		ee->kind == EXPR_LTE ||
		ee->kind == EXPR_LT ||
		ee->kind == EXPR_GT ||
		ee->kind == EXPR_GTE 
	){
		t_target.basetype = 
		type_promote(
			ee->subnodes[0]->t.basetype, 
			ee->subnodes[1]->t.basetype
		);
		
		insert_implied_type_conversion(
			ee->subnodes + 0,
			t_target
		);
		insert_implied_type_conversion(
			ee->subnodes + 1,
			t_target
		);
		return;
	}

	/*Special cases of eq and neq, add and sub where neither are pointers*/
	if(ee->kind == EXPR_ADD ||
		ee->kind == EXPR_SUB ||
		ee->kind == EXPR_EQ ||
		ee->kind == EXPR_NEQ
	)
	if(ee->subnodes[0]->t.pointerlevel == 0)
		if(ee->subnodes[1]->t.pointerlevel == 0)
		{
			t_target = type_init();
			t_target.basetype = 
			type_promote(
				ee->subnodes[0]->t.basetype, 
				ee->subnodes[1]->t.basetype
			);
			insert_implied_type_conversion(
				ee->subnodes + 0,
				t_target
			);
			insert_implied_type_conversion(
				ee->subnodes + 1,
				t_target
			);
			return;
		}
	/*streq always takes two pointers.*/
	if(ee->kind == EXPR_STREQ){
		t_target = type_init();
		t_target.basetype = BASE_U8;
		t_target.pointerlevel = 1;
		insert_implied_type_conversion(
			ee->subnodes + 0,
			t_target
		);
		insert_implied_type_conversion(
			ee->subnodes + 1,
			t_target
		);
		return;
	}
	if(ee->kind == EXPR_STRNEQ){
		t_target = type_init();
		t_target.basetype = BASE_U8;
		t_target.pointerlevel = 1;
		insert_implied_type_conversion(
			ee->subnodes + 0,
			t_target
		);
		insert_implied_type_conversion(
			ee->subnodes + 1,
			t_target
		);
		return;
	}

	/*Add Where the first one is a pointer...*/
	if(ee->kind == EXPR_ADD)
		if(ee->subnodes[0]->t.pointerlevel > 0)
		{
			//t_target.basetype = BASE_I64;
			t_target = type_init();
			t_target.basetype = SIGNED_WORD_BASE;
			t_target.pointerlevel = 0;
			insert_implied_type_conversion(
				ee->subnodes + 1,
				t_target
			);
			t_target = ee->t;
			t_target.is_lvalue = 0;
			insert_implied_type_conversion(
				ee->subnodes + 0,
				t_target
			);
			return;
		}
	/*The second...*/	
	if(ee->kind == EXPR_ADD)
		if(ee->subnodes[1]->t.pointerlevel > 0)
		{
			//t_target.basetype = BASE_I64;
			t_target = type_init();
			t_target.basetype = SIGNED_WORD_BASE;
			t_target.pointerlevel = 0;
			insert_implied_type_conversion(
				ee->subnodes + 0,
				t_target
			);
			t_target = ee->t;
			t_target.is_lvalue = 0;
			insert_implied_type_conversion(
				ee->subnodes + 1,
				t_target
			);
			return;
		}
	/*Subtracting from a pointer.*/
	if(ee->kind == EXPR_SUB)
	if(ee->subnodes[0]->t.pointerlevel > 0)
	{
		//t_target.basetype = BASE_I64;
		t_target.basetype = SIGNED_WORD_BASE;
		t_target.pointerlevel = 0;
		insert_implied_type_conversion(
			ee->subnodes + 1,
			t_target
		);
		t_target = ee->t;
		t_target.is_lvalue = 0;
		insert_implied_type_conversion(
			ee->subnodes + 0,
			t_target
		);
		return;
	}
	/*Pointer-pointer eq/neq.*/
	if(ee->kind == EXPR_EQ || ee->kind == EXPR_NEQ)
	if(ee->subnodes[0]->t.pointerlevel > 0)
	if(ee->subnodes[1]->t.pointerlevel > 0)
	{
		t_target = ee->subnodes[0]->t;
		t_target.is_lvalue = 0;
		insert_implied_type_conversion(
			ee->subnodes + 1,
			t_target
		);
		insert_implied_type_conversion(
			ee->subnodes + 0,
			t_target
		);
	}
	/*Assignment*/
	if(ee->kind == EXPR_ASSIGN){
		t_target = ee->subnodes[0]->t;
		if(t_target.is_lvalue == 0){
			puts(
				"<INTERNAL ERROR> Non-lvalue reached the lhs of assignment\n"
				"in the implied type conversion propagator."
			);
			throw_type_error("Internal error- check implied conversion propagator.");
		}
		t_target.is_lvalue = 0;
		insert_implied_type_conversion(
			ee->subnodes + 1,
			t_target
		);
		return;
	}
	if(ee->kind == EXPR_MOVE){
		t_target = ee->subnodes[0]->t;
		t_target.is_lvalue = 0;
		insert_implied_type_conversion(
			ee->subnodes + 1,
			t_target
		);
		insert_implied_type_conversion(
			ee->subnodes + 0,
			t_target
		);
		return;
	}
	
	if(ee->kind == EXPR_FCALL || ee->kind == EXPR_METHOD){
		for(i = 0; i < symbol_table[ee->symid].nargs; i++){
			type qqq;
			qqq = symbol_table[ee->symid].fargs[i][0];
			qqq.is_lvalue = 0;
			qqq.membername = NULL;
			throw_if_types_incompatible(
				ee->subnodes[i]->t,
				qqq,
				"fnptr argument is wrong."
			);
			insert_implied_type_conversion(
				ee->subnodes+i, 
				qqq
			);
		}
		return;
	}
	if(ee->kind == EXPR_GETFNPTR){
		return;
	}
	if(ee->kind == EXPR_CALLFNPTR){
		{
			type qqq;
			qqq = type_init();
			qqq.is_lvalue = 0;
			qqq.basetype = BASE_U8;
			qqq.pointerlevel = 1;
			insert_implied_type_conversion(
				ee->subnodes+0, 
				qqq
			);
		}
		if(symbol_table[ee->symid].nargs)
			for(i = 0; i < symbol_table[ee->symid].nargs; i++){
				type qqq;
				qqq = symbol_table[ee->symid].fargs[i][0];
				qqq.is_lvalue = 0;
				qqq.membername = NULL;
				throw_if_types_incompatible(
					ee->subnodes[1+i]->t,
					qqq,
					"fnptr argument is wrong."
				);
				insert_implied_type_conversion(
					ee->subnodes+1+i, 
					qqq
				);
			}
		return;
	}
	if(ee->kind == EXPR_BUILTIN_CALL){
		uint64_t got_builtin_arg1_type;
		uint64_t got_builtin_arg2_type;
		uint64_t got_builtin_arg3_type;
		nargs = get_builtin_nargs(ee->symname);
		t_target = type_init();
		/*The hardest one!!*/
		if(nargs == 0) return; /*EZ!*/
	 		got_builtin_arg1_type = get_builtin_arg1_type(ee->symname);
	 	if(nargs > 1)
			got_builtin_arg2_type = get_builtin_arg2_type(ee->symname);
		if(nargs > 2)
			got_builtin_arg3_type = get_builtin_arg3_type(ee->symname);
		/*Check argument 1.*/
		if(got_builtin_arg1_type == BUILTIN_PROTO_U8_PTR){
			t_target.basetype = BASE_U8;
			t_target.pointerlevel = 1;
		}
		if(got_builtin_arg1_type == BUILTIN_PROTO_U8_PTR2){
			t_target.basetype = BASE_U8;
			t_target.pointerlevel = 2;
		}
		if(got_builtin_arg1_type == BUILTIN_PROTO_U8_PTR2){
			t_target.basetype = BASE_U8;
			t_target.pointerlevel = 2;
		}
		if(got_builtin_arg1_type == BUILTIN_PROTO_U64_PTR){
			t_target.basetype = BASE_U64;
			t_target.pointerlevel = 1;
		}		
		if(got_builtin_arg1_type == BUILTIN_PROTO_U64){
			t_target.basetype = BASE_U64;
			t_target.pointerlevel = 0;
		}
		if(got_builtin_arg1_type == BUILTIN_PROTO_I64){
			t_target.basetype = BASE_I64;
			t_target.pointerlevel = 0;
		}
		if(got_builtin_arg1_type == BUILTIN_PROTO_DOUBLE){
			t_target.basetype = BASE_F64;
			t_target.pointerlevel = 0;
		}
		if(got_builtin_arg1_type == BUILTIN_PROTO_I32){
			t_target.basetype = BASE_I32;
			t_target.pointerlevel = 0;
		}
		insert_implied_type_conversion(
			ee->subnodes+0,
			t_target
		);
		if(nargs < 2) return;
		t_target = type_init();

		if(got_builtin_arg2_type == BUILTIN_PROTO_U8_PTR){
			t_target.basetype = BASE_U8;
			t_target.pointerlevel = 1;
		}
		if(got_builtin_arg2_type == BUILTIN_PROTO_U8_PTR2){
			t_target.basetype = BASE_U8;
			t_target.pointerlevel = 2;
		}
		if(got_builtin_arg2_type == BUILTIN_PROTO_U8_PTR2){
			t_target.basetype = BASE_U8;
			t_target.pointerlevel = 2;
		}
		if(got_builtin_arg2_type == BUILTIN_PROTO_U64_PTR){
			t_target.basetype = BASE_U64;
			t_target.pointerlevel = 1;
		}		
		if(got_builtin_arg2_type == BUILTIN_PROTO_U64){
			t_target.basetype = BASE_U64;
			t_target.pointerlevel = 0;
		}
		if(got_builtin_arg2_type == BUILTIN_PROTO_I64){
			t_target.basetype = BASE_I64;
			t_target.pointerlevel = 0;
		}
		if(got_builtin_arg2_type == BUILTIN_PROTO_DOUBLE){
			t_target.basetype = BASE_F64;
			t_target.pointerlevel = 0;
		}
		if(got_builtin_arg2_type == BUILTIN_PROTO_I32){
			t_target.basetype = BASE_I32;
			t_target.pointerlevel = 0;
		}
		insert_implied_type_conversion(
			ee->subnodes+1,
			t_target
		);
		if(nargs < 3) return;
		t_target = type_init();

		if(got_builtin_arg3_type == BUILTIN_PROTO_U8_PTR){
			t_target.basetype = BASE_U8;
			t_target.pointerlevel = 1;
		}
		if(got_builtin_arg3_type == BUILTIN_PROTO_U8_PTR2){
			t_target.basetype = BASE_U8;
			t_target.pointerlevel = 2;
		}
		if(got_builtin_arg3_type == BUILTIN_PROTO_U8_PTR2){
			t_target.basetype = BASE_U8;
			t_target.pointerlevel = 2;
		}
		if(got_builtin_arg3_type == BUILTIN_PROTO_U64_PTR){
			t_target.basetype = BASE_U64;
			t_target.pointerlevel = 1;
		}		
		if(got_builtin_arg3_type == BUILTIN_PROTO_U64){
			t_target.basetype = BASE_U64;
			t_target.pointerlevel = 0;
		}
		if(got_builtin_arg3_type == BUILTIN_PROTO_I64){
			t_target.basetype = BASE_I64;
			t_target.pointerlevel = 0;
		}
		if(got_builtin_arg3_type == BUILTIN_PROTO_DOUBLE){
			t_target.basetype = BASE_F64;
			t_target.pointerlevel = 0;
		}
		if(got_builtin_arg3_type == BUILTIN_PROTO_I32){
			t_target.basetype = BASE_I32;
			t_target.pointerlevel = 0;
		}
		insert_implied_type_conversion(
			ee->subnodes+2,
			t_target
		);
		return;
	}
	throw_type_error_with_expression_enums(
		"Implied type conversion propagator fell through! Expression kind:",
		ee->kind,EXPR_BAD
	);
}

static void validate_goto_target(stmt* me, char* name){
	int64_t i;
	uint64_t j;
	uint64_t scopediff_sofar = 0;
	uint64_t vardiff_sofar = 0;
	for(i=nscopes-1;i>=0;i--){
		stmt* stmtlist;
		stmtlist = scopestack[i]->stmts;
		for(j=0;j<scopestack[i]->nstmts;j++){
			if(stmtlist[j].kind == STMT_LABEL)
				if(streq(stmtlist[j].referenced_label_name,name))
				{
					//assign it!
					me->referenced_loop = ((stmt*)stmtlist) + j;
					//assign scopediff
					me->goto_scopediff = scopediff_sofar;
					me->goto_vardiff = vardiff_sofar;
					me->goto_where_in_scope = j;
					return;
				}
		}
		scopediff_sofar++;
		vardiff_sofar += scopestack[i]->nsyms;
	}
	puts("Goto uses out-of-sequence jump target:");
	puts(name);
	validator_exit_err();
}

static void assign_scopediff_vardiff(stmt* me, scope* jtarget_scope, int is_return){
	int64_t i;
	uint64_t scopediff_sofar = 0;
	uint64_t vardiff_sofar = 0;
	for(i=nscopes-1;i>=0;i--){
		if(jtarget_scope == scopestack[i]){
			//assign scopediff
			me->goto_scopediff = scopediff_sofar;
			me->goto_vardiff = vardiff_sofar;
			return;
		}
		scopediff_sofar++;
		vardiff_sofar += scopestack[i]->nsyms;
	}
	if(is_return){
		me->goto_scopediff = scopediff_sofar;
		me->goto_vardiff = vardiff_sofar;
		return;
	}
	puts("Validator internal error");
	puts("Could not assign scopediff/vardiff for jumping statement.");
	validator_exit_err();
}

//also does goto validation.
static void walk_assign_lsym_gsym(){
	scope* current_scope;
	unsigned long i;
	unsigned long j;

	current_scope = symbol_table[active_function].fbody;
	stmt* stmtlist;
	current_scope->walker_point = 0;
	i = 0;
	uint64_t WORD_BASE;
	uint64_t SIGNED_WORD_BASE;
	uint64_t FLOAT_BASE;
	if(symbol_table[active_function].is_codegen){
		WORD_BASE = BASE_U64;
		SIGNED_WORD_BASE = BASE_I64;
		FLOAT_BASE = BASE_F64;
	} else {
		WORD_BASE = TARGET_WORD_BASE;
		SIGNED_WORD_BASE = TARGET_WORD_SIGNED_BASE;
		FLOAT_BASE = TARGET_MAX_FLOAT_TYPE;
	}
	(void)WORD_BASE;
	//
	while(1){
		stmtlist = current_scope->stmts;
		//if this is a goto, 

		if(i >= current_scope->nstmts){
			if(nscopes <= 0) return;
			current_scope = scopestack[nscopes-1];
			scopestack_pop();
			i = current_scope->walker_point;
			/*
				if it is a loop, we have to pop from the loop stack, too
			*/
			stmtlist = current_scope->stmts;
			curr_stmt = stmtlist + i;
			if(
				stmtlist[i].kind == STMT_FOR ||
				stmtlist[i].kind == STMT_WHILE
			){
				loopstack_pop();
			}
			i++;
			continue;
		}
		curr_stmt = stmtlist + i;
		if(stmtlist[i].kind == STMT_CONTINUE ||
			stmtlist[i].kind == STMT_BREAK){
			if(nloops <= 0){
				puts("INTERNAL VALIDATOR ERROR");
				puts("Continue or break in invalid context reached code validator.");
				validator_exit_err();
			}
			if(loopstack[nloops-1] == NULL){
				puts("INTERNAL VALIDATOR ERROR");
				puts("Loopstack has a null on it?");
				validator_exit_err();
			}
			if(loopstack[nloops-1]->kind != STMT_WHILE &&
				loopstack[nloops-1]->kind != STMT_FOR
			){
				puts("INTERNAL VALIDATOR ERROR");
				puts("Loopstack has a non-loop on it?");
				validator_exit_err();
			}
			stmtlist[i].referenced_loop = loopstack[nloops-1];
			scopestack_push(current_scope);
				assign_scopediff_vardiff(
					stmtlist+i,
					stmtlist[i].referenced_loop->whereami,
					0
				);
			scopestack_pop();
		}
		if(stmtlist[i].kind == STMT_GOTO){
			int found = 0;
			for(j = 0; j < n_discovered_labels; j++)
				if(streq(stmtlist[i].referenced_label_name, discovered_labels[j]))
					found = 1;
			if(!found)
			{
				puts("Goto target:");
				puts(stmtlist[i].referenced_label_name);
				puts("Does not exist in function:");
				puts(symbol_table[active_function].name);
				validator_exit_err();

			}
			scopestack_push(current_scope);
				validate_goto_target(stmtlist + i, stmtlist[i].referenced_label_name);
			scopestack_pop();
		}

		if(stmtlist[i].nexpressions > 0){
			/*assign lsym and gsym right here.*/
			for(j = 0; j < stmtlist[i].nexpressions; j++){
				//this function needs to "see" the current scope...
				scopestack_push(current_scope);
					assign_lsym_gsym(stmtlist[i].expressions[j]);
					propagate_types(stmtlist[i].expressions[j]);
					validate_function_argument_passing(stmtlist[i].expressions[j]);
					validate_codegen_safety(stmtlist[i].expressions[j]);
					propagate_implied_type_conversions(stmtlist[i].expressions[j]);

					//Repeat for absolute safety
					propagate_types(stmtlist[i].expressions[j]);
					validate_function_argument_passing(stmtlist[i].expressions[j]);
				scopestack_pop();
			}
		}
		if(stmtlist[i].kind == STMT_SWITCH){
			type qq = ((expr_node*)stmtlist[i].expressions[0])->t;
			if(
				(qq.pointerlevel > 0) ||
				(qq.basetype == BASE_STRUCT) ||
				(qq.basetype == BASE_VOID) ||
				(qq.basetype == BASE_F64) ||
				(qq.basetype == BASE_F32) 
			)
			throw_type_error("Switch statement has non-integer expression.");
			qq = type_init();
			//qq.basetype = BASE_I64;
			qq.basetype = SIGNED_WORD_BASE;
			insert_implied_type_conversion((expr_node**)stmtlist[i].expressions, qq);
		}		
		if(stmtlist[i].kind == STMT_FOR){
			type qq = ((expr_node*)stmtlist[i].expressions[1])->t;
			if(
				(qq.pointerlevel > 0) ||
				(qq.basetype == BASE_STRUCT) ||
				(qq.basetype == BASE_VOID) ||
				(qq.basetype == BASE_F64) ||
				(qq.basetype == BASE_F32) 
			)
			throw_type_error("For statement has non-integer conditional expression..");
			qq = type_init();
			//qq.basetype = BASE_I64;
			qq.basetype = SIGNED_WORD_BASE;
			insert_implied_type_conversion((expr_node**)stmtlist[i].expressions+1, qq);
			loopstack_push(stmtlist + i);
		}	
		if(stmtlist[i].kind == STMT_WHILE){
			type qq = ((expr_node*)stmtlist[i].expressions[0])->t;
			if(
				(qq.pointerlevel > 0) ||
				(qq.basetype == BASE_STRUCT) ||
				(qq.basetype == BASE_VOID) ||
				(qq.basetype == BASE_F64) ||
				(qq.basetype == BASE_F32) 
			)
			throw_type_error("While statement has non-integer conditional expression..");
			qq = type_init();
			//qq.basetype = BASE_I64;
			qq.basetype = SIGNED_WORD_BASE;
			insert_implied_type_conversion((expr_node**)stmtlist[i].expressions, qq);
			loopstack_push(stmtlist + i);
		}	
		if(stmtlist[i].kind == STMT_IF){
			type qq = ((expr_node*)stmtlist[i].expressions[0])->t;
			if(
				(qq.pointerlevel > 0) ||
				(qq.basetype == BASE_STRUCT) ||
				(qq.basetype == BASE_VOID) ||
				(qq.basetype == BASE_F64) ||
				(qq.basetype == BASE_F32) 
			)
			throw_type_error("If statement has non-integer conditional expression..");
			qq = type_init();
			//qq.basetype = BASE_I64;
			qq.basetype = SIGNED_WORD_BASE;
			insert_implied_type_conversion((expr_node**)stmtlist[i].expressions, qq);
		}
		if(stmtlist[i].kind == STMT_ELIF){
			type qq = ((expr_node*)stmtlist[i].expressions[0])->t;
			if(qq.pointerlevel > 0)
				throw_type_error("Cannot branch on pointer in elif stmt.");
			if(qq.basetype == BASE_VOID)
				throw_type_error("Cannot branch on void in elif stmt.");
			if(
				(qq.pointerlevel > 0) ||
				(qq.basetype == BASE_STRUCT) ||
				(qq.basetype == BASE_VOID) ||
				(qq.basetype == BASE_F64) ||
				(qq.basetype == BASE_F32) 
			)
				throw_type_error("elif statement has non-integer conditional expression..");
			qq = type_init();
//			qq.basetype = BASE_I64;
			qq.basetype = SIGNED_WORD_BASE;
			insert_implied_type_conversion((expr_node**)stmtlist[i].expressions, qq);
		}
		if(stmtlist[i].kind == STMT_RETURN){
			if((expr_node*)stmtlist[i].expressions[0]){
				type qq = ((expr_node*)stmtlist[i].expressions[0])->t;
				type pp = symbol_table[active_function].t;
				pp.is_function = 0;
				pp.funcid = 0;
				throw_if_types_incompatible(pp,qq,"Return statement must have compatible type.");
				insert_implied_type_conversion((expr_node**)stmtlist[i].expressions, pp);
			}
			scopestack_push(current_scope);
				assign_scopediff_vardiff(
					stmtlist+i,
					NULL,
					1
				);
			scopestack_pop();
		}
		if(stmtlist[i].kind == STMT_TAIL){
			if(symbol_table[active_function].is_pure){
				for(i = 0; i < nsymbols; i++)
					if(symbol_table[i].name)
						if(streq(symbol_table[i].name, stmtlist[i].referenced_label_name))
							if(symbol_table[i].is_pure == 0){
								puts("Validator Error!");
								puts("Tail statement in function:");
								puts(symbol_table[active_function].name);
								puts("Tails-off to a function not explicitly defined as pure.");
								validator_exit_err();
							}
			}
			scopestack_push(current_scope);
			assign_scopediff_vardiff(
				stmtlist+i,
				NULL,
				1
			);
			scopestack_pop();
		}
		if(stmtlist[i].kind == STMT_ASM){
			symbol_table[active_function].is_impure_globals_or_asm = 1;
			symbol_table[active_function].is_impure = 1;
			if(symbol_table[active_function].is_codegen != 0){
				puts("VALIDATION ERROR!");
				puts("asm blocks may not exist in codegen functions.");
				puts("This function:");
				puts(symbol_table[active_function].name);
				puts("Was declared 'codegen' so you cannot use 'asm' blocks in it.");
				validator_exit_err();
			}
			if(symbol_table[active_function].is_pure > 0){
				puts("VALIDATION ERROR!");
				puts("asm blocks may not exist in pure functions.");
				puts("This function:");
				puts(symbol_table[active_function].name);
				puts("Was declared 'pure' so you cannot use 'asm' blocks in it.");
				validator_exit_err();
			}
		}
		if(stmtlist[i].myscope){
			/*ctx switch.*/
			current_scope->walker_point = i;
			current_scope->stopped_at_scope1 = 1;
			scopestack_push(current_scope);
			/*load scope*/
			current_scope = stmtlist[i].myscope;
			i = 0;
			continue;
		}
		i++;
	}
}



void validate_function(symdecl* funk){
	unsigned long i;
	if(funk->t.is_function == 0)
	{
		puts("INTERNAL VALIDATOR ERROR: Passed non-function.");
		validator_exit_err();
	}
	if(nscopes > 0 || nloops > 0){
		puts("INTERNAL VALIDATOR ERROR: Bad scopestack or loopstack.");
		validator_exit_err();
	}
	for(i = 0; i < nsymbols; i++){
		if(symbol_table+i == funk){
			active_function = i; break;
		}
		if(i == nsymbols-1){
			puts("INTERNAL VALIDATOR ERROR: The function is not in the symbol list!");
			exit(1);
		}
	}
	if(nsymbols == 0){
		puts("INTERNAL VALIDATOR ERROR: The function is not in the symbol list!");
		exit(1);
	}

	n_discovered_labels = 0;
	if(discovered_labels) free(discovered_labels);
	discovered_labels = NULL;
	/*1. walk through the tree and count every time a label appears.*/
	check_label_declarations(funk->fbody);
	/*2. Assign lsym and gsym to all non-function identifiers.
		this also checks to see if goto targets exist.
	*/
	walk_assign_lsym_gsym();
	if(nscopes > 0){
		puts("INTERNAL VALIDATOR ERROR: Bad scopestack after walk_assign_lsym_gsym");
		validator_exit_err();
	}	
	if(nloops > 0){
		puts("INTERNAL VALIDATOR ERROR: Bad loopstack after walk_assign_lsym_gsym");
		validator_exit_err();
	}
	/*DONE: Assign loop pointers to continue and break statements. It also does goto.*/


	n_discovered_labels = 0;
	if(discovered_labels) free(discovered_labels);
	discovered_labels = NULL;
}

