#include "stringutil.h"
#include "targspecifics.h"

#include "libmin.h"
#include "parser.h"
#include "data.h"
#include "metaprogramming.h"

char** discovered_labels = NULL;
uint64_t n_discovered_labels = 0;
static stmt* curr_stmt = NULL;

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
			puts("Column");
			puts(buf);
		}
		exit(1);
	}
}


static int this_specific_scope_has_label(char* c, scope* s){
	unsigned long i;
	stmt* stmtlist = s->stmts;
	for(i = 0; i < s->nstmts; i++)
		if(stmtlist[i].kind == STMT_LABEL)
			if(streq(c, stmtlist[i].referenced_label_name)) /*this is the label we're looking for!*/
				return 1;
	return 0;
}

static void checkswitch(stmt* sw){
	unsigned long i;
	require(sw->kind == STMT_SWITCH, "<VALIDATOR ERROR> checkswitch erroneously passed non-switch statement");

	for(i = 0; i < sw->switch_nlabels; i++)
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
	if(c == EXPR_PAREN) puts("EXPR_PAREN");
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
	if(c == EXPR_NEQ) puts("EXPR_NEQ");
	if(c == EXPR_ASSIGN) puts("EXPR_ASSIGN");
	if(c == EXPR_FCALL) puts("EXPR_FCALL");
	if(c == EXPR_BUILTIN_CALL) puts("EXPR_BUILTIN_CALL");
	if(c == EXPR_CONSTEXPR_FLOAT) puts("EXPR_CONSTEXPR_FLOAT");
	if(c == EXPR_CONSTEXPR_INT) puts("EXPR_CONSTEXPR_INT");

	if(b == EXPR_BAD) return;
	puts("b=");
	c = b;
	if(c == EXPR_PAREN) puts("EXPR_PAREN");
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
	if(c == EXPR_NEQ) puts("EXPR_NEQ");
	if(c == EXPR_ASSIGN) puts("EXPR_ASSIGN");
	if(c == EXPR_FCALL) puts("EXPR_FCALL");
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
	if(ee->kind != EXPR_EQ)
	if(ee->kind != EXPR_NEQ)
	if(ee->kind != EXPR_ADD)
	if(ee->kind != EXPR_SUB)
	if(ee->kind != EXPR_FCALL)
	if(ee->kind != EXPR_BUILTIN_CALL)
	if(ee->kind != EXPR_PRE_INCR)
	if(ee->kind != EXPR_PRE_DECR)
	if(ee->kind != EXPR_POST_INCR)
	if(ee->kind != EXPR_POST_DECR)
	if(ee->kind != EXPR_ASSIGN) /*you can assign pointers...*/
	if(ee->kind != EXPR_CAST) /*You can cast pointers...*/
	if(ee->kind != EXPR_INDEX) /*You can index pointers...*/
	if(ee->kind != EXPR_METHOD) /*You invoke methods on pointers.*/
	if(ee->kind != EXPR_MEMBER) /*You can get members of structs which are pointed to.*/
	{
		if(ee->subnodes[0])
			if(ee->subnodes[0]->t.pointerlevel > 0)
				throw_type_error_with_expression_enums(
					"Pointer math is limited to addition, subtraction, assignment, casting, equality comparison, and indexing.\n"
					"a=Parent,b=Child",
					ee->kind,
					ee->subnodes[0]->kind
				);
		if(ee->subnodes[1])
			if(ee->subnodes[1]->t.pointerlevel > 0)
				throw_type_error_with_expression_enums(
					"Pointer math is limited to addition, subtraction, assignment, casting, equality comparison, and indexing.\n"
					"a=Parent,b=Child",
					ee->kind,
					ee->subnodes[1]->kind
				);
	}


	
	if(ee->kind == EXPR_INTLIT){
		t.basetype = BASE_U64;
		ee->t = t;
		return;
	}
	if(ee->kind == EXPR_FLOATLIT){
		t.basetype = BASE_F64;
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
				if(t.is_lvalue == 0)
				{
					throw_type_error_with_expression_enums(
						"Can't have Rvalue struct! a=me",
						ee->kind,
						EXPR_BAD
					);
				}
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
				if(t.is_lvalue == 0)
				{
					throw_type_error_with_expression_enums(
						"Can't have Rvalue struct! a=me",
						ee->kind,
						EXPR_BAD
					);
				}
				t.pointerlevel = 1;
				t.is_lvalue = 0;
			}
		ee->t = t;
		ee->t.membername = NULL; /*If it was a function argument, we dont want the membername thing to be there.*/
		return;
	}
	if(ee->kind == EXPR_SIZEOF){
		t.basetype = BASE_U64;
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
		ee->t.basetype = BASE_F64;
		return;
	}
	if(ee->kind == EXPR_CONSTEXPR_INT){
		ee->t.basetype = BASE_I64;
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
		t.is_lvalue = 1; /*if it wasn't an lvalue before, it was now!*/
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
		int found = 0;
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
				found = 1;
				ee->t = type_table[t.structid].members[j];
				//handle: struct member is array.
				if(ee->t.arraylen){
					ee->t.is_lvalue = 0;
					ee->t.arraylen = 0;
					ee->t.pointerlevel++;
				}
				ee->t.membername = NULL; /*We don't want it!*/
				return;
			}
		}{
			puts("Struct:");
			puts(type_table[t.structid].name);
			puts("Does not have member:");
			puts(ee->symname);
			throw_type_error_with_expression_enums(
				"Struct lacking member. a=me",
				ee->kind,
				EXPR_BAD
			);
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
		c = strcataf1(c, "_");
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
				t.is_function = 0;
				t.funcid = 0;
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
			throw_type_error("EXPR_FCALL erroneously has bad symbol ID. This should have been resolved in parser.c");
		}
		ee->t = symbol_table[ee->symid].t;
		ee->t.is_function = 0;
		ee->t.funcid = 0;
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
		ee->t.basetype = BASE_U64;
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
		ee->t = ee->subnodes[0]->t; /*it assigns, and then gets that value.*/
		return;
	}
	if(ee->kind == EXPR_NEG){
		t = ee->subnodes[0]->t;
		if(t.pointerlevel > 0)
			throw_type_error("Cannot negate pointer.");
		if(
			t.basetype == BASE_U8 ||
			t.basetype == BASE_U16 ||
			t.basetype == BASE_U32 ||
			t.basetype == BASE_U64
		) t.basetype = BASE_I64;
		ee->t = t;
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
		t.basetype = BASE_I64;
		ee->t = t;
		return;
	}
	if(ee->kind == EXPR_MUL ||
		ee->kind == EXPR_DIV
	){
		t = ee->subnodes[0]->t;
		t2 = ee->subnodes[1]->t;
		if(t.pointerlevel > 0 ||
			t2.pointerlevel > 0)
			throw_type_error_with_expression_enums(
				"Cannot do multiplication, division, or modulo on a pointer.",
				ee->subnodes[0]->kind,
				ee->subnodes[1]->kind
			);
		ee->t.basetype = type_promote(t.basetype, t2.basetype);
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
		return;
	}
	if(ee->kind == EXPR_ADD){
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
			return;
		}
		if(t2.pointerlevel > 0 && t.pointerlevel == 0){
			if(t.basetype == BASE_F32 || t.basetype == BASE_F64)
				throw_type_error("Cannot add float to pointer.");
			ee->t = t2;
			return;
		}
		ee->t.basetype = type_promote(t.basetype, t2.basetype);
		return;
	}

	
	if(ee->kind == EXPR_SUB){
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
		return;
	}
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
		if(got_builtin_arg1_type == BUILTIN_PROTO_I32){
			t_target.basetype = BASE_I32;
			t_target.pointerlevel = 0;
		}
		throw_if_types_incompatible(
			t_target, ee->subnodes[0]->t, 
			"First argument passed to builtin function is wrong!"
		);
		t_target = type_init();
		if(nargs < 2) return;
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
		if(got_builtin_arg2_type == BUILTIN_PROTO_I32){
			t_target.basetype = BASE_I32;
			t_target.pointerlevel = 0;
		}
		throw_if_types_incompatible(
			t_target, 
			ee->subnodes[1]->t, 
			"Second argument passed to builtin function is wrong!"
		);
	}

	if(ee->kind == EXPR_FCALL || ee->kind == EXPR_METHOD){
		char buf_err[3000];
		strcpy(buf_err, "Function Argument Number ");

		
		nargs = symbol_table[ee->symid].nargs;
		for(i = 0; i < nargs; i++){
			if(ee->kind == EXPR_FCALL)
				strcpy(buf_err, "fn arg ");
			if(ee->kind == EXPR_METHOD)
				strcpy(buf_err, "method arg ");
			mutoa(buf_err + strlen(buf_err), i+1);
			strcat(buf_err, " has the wrong type.\n");
			if(ee->kind == EXPR_METHOD)
				strcat(
					buf_err,
					"Note that argument number includes 'this' as the invisible first argument."
				);
			throw_if_types_incompatible(
				symbol_table[ee->symid].fargs[i][0], 
				ee->subnodes[i]->t,
				buf_err
			);
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
		ee->kind == EXPR_METHOD
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

//also does goto validation.
static void walk_assign_lsym_gsym(){
	scope* current_scope;
	unsigned long i;
	unsigned long j;

	current_scope = symbol_table[active_function].fbody;
	stmt* stmtlist;
	current_scope->walker_point = 0;
	i = 0;

	//
	while(1){
		stmtlist = current_scope->stmts;
		//if this is a goto, 

		if(i >= current_scope->nstmts){
			if(nscopes <= 0) return;
			current_scope = scopestack[nscopes-1];
			scopestack_pop();
			i = current_scope->walker_point;
			i++;
			continue;
		}
		curr_stmt = stmtlist + i;
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
				exit(1);
			}
		}

		if(stmtlist[i].nexpressions > 0){
			/*assign lsym and gsym right here.*/
			for(j = 0; j < stmtlist[i].nexpressions; j++){
				//this function needs to "see" the current scope...
				scopestack_push(current_scope);
				assign_lsym_gsym(stmtlist[i].expressions[j]);
				propagate_types(stmtlist[i].expressions[j]);
				validate_function_argument_passing(stmtlist[i].expressions[j]);
				/*
					TODO: Check function arguments for type compatibility.
				*/
				validate_codegen_safety(stmtlist[i].expressions[j]);
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
		}
		if(stmtlist[i].kind == STMT_RETURN){
			if((expr_node*)stmtlist[i].expressions[0]){
				type qq = ((expr_node*)stmtlist[i].expressions[0])->t;
				type pp = symbol_table[active_function].t;
				pp.is_function = 0;
				pp.funcid = 0;
				throw_if_types_incompatible(pp,qq,"Return statement must have compatible type.");
			}
		}
		if(stmtlist[i].kind == STMT_ASM){
			symbol_table[active_function].is_impure_globals_or_asm = 1;
			symbol_table[active_function].is_impure = 1;
			if(symbol_table[active_function].is_codegen != 0){
				puts("VALIDATION ERROR!");
				puts("Assembly blocks may not exist in codegen functions.");
				puts("This function:");
				puts(symbol_table[active_function].name);
				puts("Was declared 'codegen' so you cannot use 'asm' blocks in it.");
				validator_exit_err();
			}
		}
		if(stmtlist[i].myscope){
			/*Immediately switch.*/
			current_scope->walker_point = i;
			current_scope->stopped_at_scope1 = 1;
			scopestack_push(current_scope);
			current_scope = stmtlist[i].myscope;
			i = 0;
			continue;
		}
		i++;
	}
}



void validate_function(symdecl* funk){
	if(funk->t.is_function == 0)
	{
		puts("INTERNAL VALIDATOR ERROR: Passed non-function.");
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

	/*
		(DONE)
		1. enforce switch taking an integer type as its argument.
		2. enforce return statement type.
	*/

	/*
		TODO:
		determine is_pure. 
		Must be done at end of compilation when all symbols are defined,
		so that other functions can be assessed as well.
	*/

	n_discovered_labels = 0;
	if(discovered_labels) free(discovered_labels);
	discovered_labels = NULL;
}

int compilation_unit_determine_purity_iteration(){
	
}


/*
	TODO:
	determine is_pure. 
	repeatedly try to check every single function for impurity.
	if an iteration changed purity, then the whole unit must be done again.
*/

void compilation_unit_calculate_purity(){
	
}