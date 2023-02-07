#ifndef DATA_H
#define DATA_H

/*
	DATA.H

	defines the data structures needed during compilation of a cbas program into either IR or target code.

*/


#include "targspecifics.h"
#include <stdlib.h>
#include "parser.h"



enum {
	BASE_VOID=0,
	BASE_U8=1,
	BASE_I8=2,
	BASE_U16=3,
	BASE_I16=4,
	BASE_U32=5,
	BASE_I32=6,
	BASE_U64=7,
	BASE_I64=8,
	BASE_F32=9,
	BASE_F64=10,
	BASE_STRUCT=11,
	BASE_FUNCTION=12,
	NBASETYPES=13
};

static inline uint64_t type_promote(uint64_t a, uint64_t b){

	uint64_t promoted_basetype = a;
	uint64_t weaker_basetype = b;
	if(
		(b >= BASE_STRUCT) ||
		(a >= BASE_STRUCT)
	){
		puts("INTERNAL ERROR! Tried to promote struct or bigger!");
		exit(1);
	}
	if(b == BASE_VOID ||
	a == BASE_VOID){
		puts("INTERNAL ERROR! Tried to promote void!");
		exit(1);
	}

	if(b > a){
		promoted_basetype = b;
		weaker_basetype = a;
	}


	
	if(
		promoted_basetype == BASE_U8 ||
		promoted_basetype == BASE_U16 ||
		promoted_basetype == BASE_U32 ||
		promoted_basetype == BASE_U64
	)
		if(
			weaker_basetype == BASE_I8 ||
			weaker_basetype == BASE_I16 ||
			weaker_basetype == BASE_I32 ||
			weaker_basetype == BASE_I64 
		)
			promoted_basetype++; //become signed.
	return promoted_basetype;

}


/*the "type" struct.*/
typedef struct type{
	uint64_t basetype; /*value from the above enum. Not allowed to be BASE_FUNCTION.*/
	uint64_t pointerlevel;
	uint64_t arraylen;
	uint64_t structid; /*from the list of typedecls.*/
	uint64_t is_lvalue; /*useful in expressions.*/

	/*everything above was for the return type if this is a function...*/
	uint64_t funcid; /*if this is a function, what is the function ID? From the list of symdecls.*/
	uint64_t is_function; /*
		is this a function? if this is set, 
		all the "normal" type information here is 
		interpreted as describing the 
		return type.
	*/
	char* membername; /*used for struct members and function arguments*/
	/*Code generator data.*/
	uint8_t* cgen_udata;
	uint64_t VM_function_stackframe_placement; /*For function args, used by the AST executor*/
}type;



typedef struct{
	char* name;
	struct type* members;
	uint64_t nmembers;
	uint64_t is_incomplete; /*incomplete struct declaration*/
	/*Code generator data.*/
	uint8_t* cgen_udata;
} typedecl;


/*Functions and Variables.*/
typedef struct{
	type t;
	char* name;
	type* fargs[MAX_FARGS]; /*function arguments*/
	uint64_t nargs; 		/*Number of arguments to function*/
	void* fbody; /*type is scope*/
	uint8_t* cdata;
	uint64_t cdata_sz;
	uint64_t is_pub;
	uint64_t is_incomplete; /*incomplete symbol, such as a predeclaration or extern.*/
	uint64_t is_codegen; /*Compiletime only?*/
	uint64_t is_impure; /*exactly what it says*/
	uint64_t is_impure_globals_or_asm; /*contains impure behavior.*/
	uint64_t is_impure_uses_incomplete_symbols; /*uses incomplete symbols.*/
	/*Code generator data.*/
	uint8_t* cgen_udata;
	uint64_t VM_function_stackframe_placement; /*For local variables and function args, used by the AST executor*/
} symdecl;


static inline int sym_is_method(symdecl* s){
	if(s->t.is_function == 0) return 0;
	if(s->fargs[0] == NULL) return 0;
	if(s->fargs[0]->membername == NULL) return 0;
	if(streq(s->fargs[0]->membername, "this")){
		if(s->fargs[0]->pointerlevel == 1)
			if(s->fargs[0]->basetype == BASE_STRUCT)
				return 1;
	}
	return 0;
}

/*
	Scopes, or classes.
*/
typedef struct scope{
	symdecl* syms;
	uint64_t nsyms;
	void* stmts; /**/
	uint64_t nstmts;
	uint64_t is_fbody;
	uint64_t is_loopbody;
	/*Code generator data.*/
	uint8_t* cgen_udata;
	uint64_t walker_point; /*Where was the code validator?*/
	uint8_t stopped_at_scope1; /*Did the validator stop at myscope or myscope2?*/
} scope;

enum{
	STMT_BAD=0,
	STMT_NOP, /*of the form ;, also generated after every single 'end' so that a loop */
	STMT_EXPR,
	STMT_LABEL,
	STMT_GOTO,
	STMT_GOTO_,
	STMT_WHILE,
	STMT_FOR,
	STMT_IF,
	STMT_RETURN,
	STMT_TAIL,
	STMT_ASM,
	STMT_CONTINUE,
	STMT_BREAK,
	STMT_SWITCH,
	NSTMT_TYPES
};

#define STMT_MAX_EXPRESSIONS 8
typedef struct stmt{
	scope* whereami; /*scope this statement is in. Not owning.*/
	scope* myscope; /*if this statement has a scope after it (while, for, if, etc) then this is where it goes.*/
	uint64_t kind; /*What type of statement is it?*/
	/*expressions belonging to this statement.*/
	void* expressions[STMT_MAX_EXPRESSIONS];
	uint64_t nexpressions;
	struct stmt* referenced_loop; /*
		break and continue reference 
		a loop construct that they reside in.

		What loop exactly has to be determined in a second pass,
		since the stmt lists are continuously re-alloced.

	*/
	char* referenced_label_name; /*goto gets a label. tail gets a function*/
	char** switch_label_list;
	uint64_t switch_nlabels; /*how many labels does the switch have in it?*/
	/*Code generator data.*/
	uint8_t* cgen_udata;
	/*Used for error printing...*/
	uint64_t linenum;
	uint64_t colnum;
	char* filename;
} stmt;

enum{
	EXPR_BAD=0,
	EXPR_BUILTIN_CALL,
	EXPR_FCALL,
	EXPR_PAREN, /*Encapsulated expression. Just adds a level of indirection, basically.*/
	EXPR_SIZEOF, /*takes a type.*/
	EXPR_INTLIT,
	EXPR_FLOATLIT,
	EXPR_STRINGLIT, /*references a global symbol, identical to the gsym*/
	EXPR_LSYM, /*Local symbol- declared within scope hierarchy or function arguments.*/
	EXPR_GSYM, /*Global symbol- declared at global scope. */
	EXPR_SYM, /*Unidentified symbol- unknown type.*/
	/*unary postfix operators*/
	EXPR_POST_INCR,
	EXPR_POST_DECR,
	EXPR_INDEX,
	EXPR_MEMBER, /*.myMember, ->myMember is not supported.*/
	EXPR_METHOD, /*:myMethod(), the name mangling has to happen during the second pass.*/
	/*unary prefix operators*/
	EXPR_CAST,
	EXPR_NEG,
	EXPR_COMPL,
	EXPR_NOT,
	EXPR_PRE_INCR,
	EXPR_PRE_DECR,
	/*binary operators starting at the bottom*/
	EXPR_MUL,
	EXPR_DIV,
	EXPR_MOD,
	
	EXPR_ADD,
	EXPR_SUB,
	EXPR_BITOR,
	EXPR_BITAND,
	EXPR_BITXOR,
	EXPR_LSH,
	EXPR_RSH,
	EXPR_LOGOR,
	EXPR_LOGAND,
	EXPR_LT,
	EXPR_GT,
	EXPR_LTE,
	EXPR_GTE,
	EXPR_EQ,
	EXPR_NEQ,
	EXPR_ASSIGN,
	EXPR_MOVE,
	EXPR_CONSTEXPR_FLOAT,
	EXPR_CONSTEXPR_INT,
	NEXPR_TYPES
};

typedef struct expr_node{
	type t;
	type type_to_get_size_of; //sizeof and cast both use this.
	uint64_t kind;
	double fdata;
	uint64_t idata;
	struct expr_node* subnodes[MAX_FARGS];
	uint64_t symid;
	uint64_t is_global_variable;
	uint64_t is_function;
	uint64_t is_local_variable;
	char* referenced_label_name;
	char* symname;  /*if method: this is mangled. */
	char* method_name; /*if method: this is unmangled. */
	/*Code generator data.*/
	uint8_t* cgen_udata;
}expr_node;

static expr_node expr_node_init(){
	expr_node ee = {0};
	return ee;
}
static void expr_node_destroy(expr_node* ee){
	uint64_t i;
	for(i = 0; i < MAX_FARGS; i++){
		if(ee->subnodes[i]){
			expr_node_destroy(ee->subnodes[i]);
			free(ee->subnodes[i]);
		}
	}
	if(ee->referenced_label_name)
		free(ee->referenced_label_name);
	if(ee->symname)
		free(ee->symname);
	*ee = expr_node_init();
}
static stmt stmt_init(){
	stmt s = {0};
	return s;
}
static scope scope_init(){
	scope s = {0};
	return s;
}
static void scope_destroy(scope* s);
static void stmt_destroy(stmt* s){
	uint64_t i;
	for(i = 0; i < s->nexpressions; i++)
		expr_node_destroy(s->expressions[i]);
	if(s->myscope)
		scope_destroy(s->myscope);	
	/*if(s->myscope2)
		scope_destroy(s->myscope2);*/
	if(s->referenced_label_name)
		free(s->referenced_label_name);
	*s = stmt_init();
}



#define MAX_TYPE_DECLARATIONS 65536
#define MAX_SYMBOL_DECLARATIONS 65536
#define MAX_SCOPE_DEPTH 65536


extern typedecl* type_table;
extern symdecl* symbol_table;
extern scope** scopestack;
extern stmt** loopstack;
extern uint64_t active_function; //What function are we currently working in? Used to get function arguments.
extern uint64_t ntypedecls;
extern uint64_t nsymbols;
extern uint64_t nscopes;
extern uint64_t nloops; /*Needed for identifying the parent loop.*/

static inline int peek_is_fname(){
	if(peek()->data != TOK_IDENT) return 0;
	if(nsymbols == 0) return 0;
	for(unsigned long i = 0; i < nsymbols; i++){
		if(streq(peek()->text, symbol_table[i].name)){
			if(symbol_table[i].t.is_function)
				return 1;
		}
	}
	return 0;
}
static inline int str_is_fname(char* s){
	for(unsigned long i = 0; i < nsymbols; i++){
		if(streq(s, symbol_table[i].name)){
			if(symbol_table[i].t.is_function)
				return 1;
		}
	}
	return 0;
}

static inline int ident_is_used_locally(char* s){
	for(unsigned long i = 0; i < nscopes; i++)
		for(unsigned long j = 0; j < scopestack[i]->nsyms; j++)
			if(streq(s, scopestack[i]->syms[j].name))
				return 1;
	
	for(unsigned long i = 0;i < symbol_table[active_function].nargs;i++){
		if(streq(s, symbol_table[active_function].fargs[i]->membername))
			return 1;
	}
	return 0;
}


/*Used during initial-pass parsing for error checking. It is insufficient to prevent duplicate labels in a function, ala:
	fn myFunc():
		:myLabel2
		if(1)
			:myLabel //I see nothing!
		end
		if(1)
			:myLabel //I see nothing as well!
		end
		while(0)
			:myLabel //Wow, there are a lot of us!
		end
	end

*/
static inline int label_name_is_already_in_use(char* s){
	for(unsigned long i = 0; i < nscopes; i++)
		for(unsigned long j = 0; j < scopestack[i]->nstmts; j++){
			stmt* in_particular = ((stmt*)scopestack[i]->stmts) + j;
			if( in_particular->kind == STMT_LABEL)
				if( in_particular->referenced_label_name )
					if(streq(s, in_particular->referenced_label_name)) return 1;
		}
	return 0;
}

static inline void scopestack_push(scope* s){
	scopestack = realloc(scopestack, (++nscopes) * sizeof(scope*));
	require(scopestack != NULL, "Malloc failed");
	scopestack[nscopes-1] = s;
}

static inline void scopestack_pop(){
	if(nscopes == 0)
		parse_error("Tried to pop scope when there was none to pop!");
	nscopes--;
}

static inline void loopstack_push(stmt* s){
	loopstack = realloc(loopstack, (++nloops) * sizeof(stmt*));
	require(loopstack != NULL, "Malloc failed");
	loopstack[nloops-1] = s;
}

static inline void loopstack_pop(){
	if(nloops == 0)
		parse_error("Tried to pop loop when there was none to pop!");
	nloops--;
}


static inline int type_can_be_assigned_integer_literal(type t){
	if(t.arraylen) return 0;
	if(t.pointerlevel) return 1;
	if(t.basetype == BASE_VOID) return 0;
	if(t.basetype >= BASE_STRUCT) return 0;
	return 1;
}

static inline int type_can_be_assigned_float_literal(type t){
	if(t.arraylen) return 0;
	if(t.pointerlevel) return 1;
	if(t.basetype == BASE_VOID) return 0;
	if(t.basetype >= BASE_STRUCT) return 0;
	return 1;
}
static inline int type_should_be_assigned_float_literal(type t){
	if(t.arraylen) return 0;
	if(t.pointerlevel) return 0;
	if(t.basetype == BASE_F64 || 
		t.basetype == BASE_F32) return 1;
	return 0;
}

static inline int peek_ident_is_already_used_globally(){
	uint64_t i;
	if(peek()->data != TOK_IDENT) return 0;
	for(i = 0; i < nsymbols; i++)
		if(streq(peek()->text, symbol_table[i].name))
			return 1;	
	if(peek_is_typename()) return 1;

	return 0;
}

static inline int ident_is_already_used_globally(char* name){
	uint64_t i;
	for(i = 0; i < nsymbols; i++)
		if(
			streq(name, 
				symbol_table[i].name
			)
		)
			return 1;	
	for(i = 0; i < ntypedecls; i++)
		if(streq(name, type_table[i].name))
			return 1;
	return 0;
}

static inline int type_can_be_in_expression(type t){
	if(t.arraylen > 0) return 0;
	if(t.pointerlevel > 0) return 1;
	if(t.basetype >= BASE_STRUCT) return 0;
	return 1;
}


static inline int type_can_be_literal(type t){
	if(t.arraylen > 0) return 0;
	if(t.pointerlevel > 0) return 0;
	if(t.basetype >= BASE_STRUCT) return 0;
	if(t.basetype == BASE_VOID) return 0;
	return 1;
}

static inline int type_can_be_variable(type t){
	if(t.is_function) return 0; /*You may not declare a variable whose type is a function. Functions are functions, not variables!*/
	if(t.basetype == BASE_VOID && t.pointerlevel == 0) return 0; /*You may not declare a variable whose type is void.*/
	if(t.basetype == BASE_STRUCT){
		if(t.structid >= ntypedecls) /*You may not have an invalid struct ID.*/
			return 0;
	}
	if(t.basetype > BASE_STRUCT) return 0; /*invalid basetype*/
	if(t.pointerlevel > 65536) return 0; /*May not declare more than 65536 levels of indirection. Mostly as a sanity check.*/
	if(t.arraylen && t.is_lvalue) return 0; /*Arrays are never lvalues.*/
	/*Otherwise: Yes. it is valid.*/
	return 1;
}


static inline uint64_t type_getsz(type t){
	if(t.arraylen){
		type q = {0};
		q = t;
		q.arraylen = 0;
		return t.arraylen * type_getsz(q);
	}
	if(t.pointerlevel) return POINTER_SIZE;
	if(t.basetype == BASE_STRUCT){
		unsigned long sum = 0;
		if(t.structid > ntypedecls)
			parse_error("Asked to determine size of non-existent struct type?");
		/*go through the members and add up how big each one is.*/
		for(unsigned long i = 0; i < type_table[t.structid].nmembers;i++)
			sum += type_getsz(type_table[t.structid].members[i]);
		return sum;
	}
	if(t.basetype == BASE_U8) return 1;
	if(t.basetype == BASE_I8) return 1;
	if(t.basetype == BASE_U16) return 2;
	if(t.basetype == BASE_I16) return 2;	
	if(t.basetype == BASE_U32) return 4;
	if(t.basetype == BASE_I32) return 4;
	if(t.basetype == BASE_F32) return 4;	
	if(t.basetype == BASE_U64) return 8;
	if(t.basetype == BASE_I64) return 8;
	if(t.basetype == BASE_F64) return 8;
	
	if(t.basetype == BASE_VOID) return 0;
	return 0;
}



static inline type type_init(){
	type t = {0};
	return t;
}
static inline void type_destroy(type* t){
	/*always free membername...*/
	if(t->membername) free(t->membername);
	*t = type_init();
}



static inline symdecl symdecl_init(){
	symdecl s = {0}; return s;
}

static inline void symdecl_destroy(symdecl* t){
	unsigned long i;
	type_destroy(&t->t);
	for(i = 0; i < MAX_FARGS; i++){
		if(t->fargs[i]){
			type_destroy(t->fargs[i]);
			free(t->fargs[i]);
			t->fargs[i] = NULL;
		}
	}
	if(t->name)free(t->name);
	if(t->fbody) scope_destroy(t->fbody);
	if(t->cdata) free(t->cdata);
	*t = symdecl_init();
}

static inline typedecl typedecl_init(){
	typedecl s = {0}; return s;
}

/*Type declarations */
static inline void typedecl_destroy(typedecl* t){
	/*Type declarations own BOTH fargs and members.*/
	if(t->members){
		for(size_t i = 0; i < t->nmembers; i++)
			type_destroy(t->members + i);
		free(t->members);
	}
	if(t->name)free(t->name);
	*t = typedecl_init();
}




static void scope_destroy(scope* s){
	if(s->syms){
		for(unsigned long i = 0; i < s->nsyms; i++)
			symdecl_destroy(&s->syms[i]);
		free(s->syms);
	}
	for(unsigned long i = 0; i < s->nstmts; i++)
		stmt_destroy(s->stmts+i);
	*s = scope_init();
	return;
}


static inline int peek_match_keyw(char* s){
	if(peek()->data != TOK_KEYWORD) return 0;
	return (ID_KEYW(peek()) == ID_KEYW_STRING(s));
}

static inline int strll_match_keyw(strll* i, char* s){
	if(peek()->data != TOK_KEYWORD) return 0;
	return (ID_KEYW(i) == ID_KEYW_STRING(s));
}


type parse_type();

#endif
