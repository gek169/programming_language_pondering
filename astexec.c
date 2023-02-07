


#include "stringutil.h"
#include "targspecifics.h"

#include "libmin.h"
#include "parser.h"
#include "data.h"
#include "metaprogramming.h"
#include "astexec.h"

/*
	VM Stack, used for the codegen stack.
*/

static ast_vm_stack_elem vm_stack[0x10000];
static uint64_t vm_stackpointer = 0;

/*
	How much memory is the frame using?
	what about the expression?
*/
uint64_t cur_func_frame_size = 0;
uint64_t cur_expr_stack_usage = 0;

static uint64_t ast_vm_stack_push(){
	return vm_stackpointer++;
}
static uint64_t ast_vm_stack_push_lvar(symdecl* s){
	uint64_t placement = ast_vm_stack_push();
	vm_stack[placement].vname = s->name;
	vm_stack[placement].identification = VM_VARIABLE;
	vm_stack[placement].t = s->t; //NOTE: Includes lvalue information.
	if(s->t.arraylen){
		vm_stack[placement].ldata = calloc(type_getsz(s->t),1);
	}
	/*structs also get placement on the stack.*/
	if(s->t.arraylen == 0)
	if(s->t.pointerlevel == 0)
	if(s->t.basetype == BASE_STRUCT){
		vm_stack[placement].ldata = calloc(type_getsz(s->t),1);
	}
	return placement;
}

static uint64_t ast_vm_stack_push_temporary(char* name, type t){
	uint64_t placement = ast_vm_stack_push();
	vm_stack[placement].vname = name;
	vm_stack[placement].ldata = NULL; //forbidden to have a struct or 
	if(t.arraylen){
		t.pointerlevel++;
		t.arraylen = 0;
		t.is_lvalue  = 0;
	}
	if(t.arraylen == 0)
	if(t.pointerlevel == 0)
	if(t.basetype == BASE_STRUCT){
		if(t.is_lvalue == 0){
			puts("<VM ERROR>");
			puts("Struct Rvalue temporary?");
			exit(1);
		}
		t.pointerlevel = 1;
	}
	vm_stack[placement].t = t;
	return placement;
}

static void ast_vm_stack_pop(){
	if(vm_stackpointer == 0){
		puts("<VM ERROR> stack was popped while empty.");
		exit(1);
	}
	if(vm_stack[vm_stackpointer-1].ldata)
		free(vm_stack[vm_stackpointer-1].ldata);
	vm_stack[vm_stackpointer-1].ldata = 0;
	vm_stack[vm_stackpointer-1].vname = NULL; /*Don't want to accidentally pick this up in the future!*/
	vm_stackpointer--;
}


/*math table*/
static int64_t do_ilt(int64_t a, int64_t b){return a < b;}
static int64_t do_ult(uint64_t a, uint64_t b){return a < b;}
static int64_t do_flt(float a, float b){return a < b;}
static int64_t do_dlt(double a, double b){return a < b;}

static int64_t do_ilte(int64_t a, int64_t b){return a <= b;}
static int64_t do_ulte(uint64_t a, uint64_t b){return a <= b;}
static int64_t do_flte(float a, float b){return a <= b;}
static int64_t do_dlte(double a, double b){return a <= b;}

static int64_t do_igt(int64_t a, int64_t b){return a > b;}
static int64_t do_ugt(uint64_t a, uint64_t b){return a > b;}
static int64_t do_fgt(float a, float b){return a > b;}
static int64_t do_dgt(double a, double b){return a > b;}


static int64_t do_igte(int64_t a, int64_t b){return a >= b;}
static int64_t do_ugte(uint64_t a, uint64_t b){return a >= b;}
static int64_t do_fgte(float a, float b){return a >= b;}
static int64_t do_dgte(double a, double b){return a >= b;}


static int64_t do_ieq(int64_t a, int64_t b){return a == b;}
static int64_t do_ueq(uint64_t a, uint64_t b){return a == b;}
static int64_t do_feq(float a, float b){return a == b;}
static int64_t do_deq(double a, double b){return a == b;}

static int64_t do_ineq(int64_t a, int64_t b){return a == b;}
static int64_t do_uneq(uint64_t a, uint64_t b){return a == b;}
static int64_t do_fneq(float a, float b){return a == b;}
static int64_t do_dneq(double a, double b){return a == b;}


static int64_t do_iadd(int64_t a, int64_t b){return a + b;}
static uint64_t do_uadd(uint64_t a, uint64_t b){return a + b;}
static float do_fadd(float a, float b){return a + b;}
static double do_dadd(double a, double b){return a + b;}

static int64_t do_isub(int64_t a, int64_t b){return a - b;}
static uint64_t do_usub(uint64_t a, uint64_t b){return a - b;}
static float do_fsub(float a, float b){return a - b;}
static double do_dsub(double a, double b){return a - b;}

static int64_t do_imul(int64_t a, int64_t b){return a * b;}
static uint64_t do_umul(uint64_t a, uint64_t b){return a * b;}
static float do_fmul(float a, float b){return a * b;}
static double do_dmul(double a, double b){return a * b;}

static int64_t do_idiv(int64_t a, int64_t b){return a / b;}
static uint64_t do_udiv(uint64_t a, uint64_t b){return a / b;}
static float do_fdiv(float a, float b){return a / b;}
static double do_ddiv(double a, double b){return a / b;}

static int64_t do_idecr(int64_t a){return a-1;}
static uint64_t do_udecr(uint64_t a){return a-1;}
static float do_fdecr(float a){return a-1;}
static double do_ddecr(double a){return a-1;}

static int64_t do_iincr(int64_t a){return a+1;}
static uint64_t do_uincr(uint64_t a){return a+1;}
static float do_fincr(float a){return a+1;}
static double do_dincr(double a){return a+1;}

static int64_t do_ineg(int64_t a){return -a;}
static int64_t do_uneg(uint64_t a){return -a;}
static float do_fneg(float a){return -a;}
static double do_dneg(double a){return -a;}

static int64_t do_inot(int64_t a){return !a;}
static int64_t do_unot(uint64_t a){return !a;}
static int64_t do_fnot(float a){return !a;}
static int64_t do_dnot(double a){return !a;}

static int64_t do_icompl(int64_t a){return ~a;}
static int64_t do_ucompl(uint64_t a){return ~a;}
static int64_t do_fcompl(float a){return ~(int64_t)a;}
static int64_t do_dcompl(double a){return ~(int64_t)a;}

static int64_t do_imod(int64_t a, int64_t b){return a % b;}
static uint64_t do_umod(uint64_t a, uint64_t b){return a % b;}

static int64_t do_lsh(int64_t a, int64_t b){return a<<b;}
static int64_t do_rsh(int64_t a, int64_t b){return a>>b;}
static int64_t do_bitand(int64_t a, int64_t b){return a&b;}
static int64_t do_bitor(int64_t a, int64_t b){return a|b;}
static int64_t do_bitxor(int64_t a, int64_t b){return a ^ b;}
static int64_t do_logand(int64_t a, int64_t b){return a && b;}
static int64_t do_logor(int64_t a, int64_t b){return a || b;}

static void* do_ptradd(char* ptr, int64_t num, uint64_t sz){
	return ptr + num * sz;
}
/*a
static float do_fmod(float a, float b){return a % b;}
static double do_dmod(double a, double b){return a % b;}
*/


static uint64_t determine_scope_usage(scope* s){
	uint64_t i;
	uint64_t my_usage = 0;
	uint64_t max_child_usage = 0;
	uint64_t cur_child_usage = 0;
	stmt* stmtlist;
	my_usage = s->nsyms; /*arrays are a single element.*/
	stmtlist = s->stmts;
	for(i = 0; i < s->nstmts; i++){
		if(stmtlist[i].myscope){
			cur_child_usage = determine_scope_usage(stmtlist[i].myscope);
			if(cur_child_usage > max_child_usage)
				max_child_usage = cur_child_usage;
		}
	}
	return my_usage + max_child_usage;
}


uint64_t determine_fn_variable_stack_placements(symdecl* s){
	int64_t i;
	uint64_t frame_size = 1;
	require(s->t.is_function, "AST Executor: Asked to determine placement of variables in a non-function.");
	require(s->is_incomplete == 0, "AST Executor: Asked to determine stack placement of variables in incomplete function.");
	require(s->fbody != NULL, "AST Executor: Fbody is NULL");
	
	/*we add one because we leave an FFRAME_BEGIN magic on the stack when we do a call.*/
	frame_size = 1 + determine_scope_usage(s->fbody);

	for(i = 0; i < (int64_t)s->nargs; i++){
		s->fargs[i]->VM_function_stackframe_placement = 
			-1 * frame_size + //usage of our fbody as well as all its children.
			-1 * (i)  //each arguments is incrementally deeper into the stack.
		;
	}
	return frame_size;
}

/*Get a pointer to the memory!*/
static void* retrieve_variable_memory_pointer(
	char* name, 
	int is_global
){
	int64_t i;
	int64_t j;
	if(is_global){
		for(i = 0; i < (int64_t)nsymbols; i++)
			if(streq(symbol_table[i].name,name)){
				if(symbol_table[i].is_incomplete){
					puts("VM Error");
					puts("This global:");
					puts(name);
					puts("Was incomplete at access time.");
					exit(1);
				}
				if(symbol_table[i].t.is_function){
					puts("VM Error");
					puts("This global:");
					puts(name);
					puts("Was accessed as a variable, but it's a function!");
					exit(1);
				}
				/*if it has no cdata- initialize it!*/
				if(symbol_table[i].cdata == NULL){
					uint64_t sz = type_getsz(symbol_table[i].t);
					symbol_table[i].cdata = calloc(
						sz,1
					);
					if(symbol_table[i].cdata == NULL){
						puts("failed trying to calloc a global variable's cdata.");
						exit(1);
					}
					symbol_table[i].cdata_sz = sz;
				}
				return symbol_table[i].cdata;
		}
		puts("VM ERROR");
		puts("Could not find global:");
		puts(name);
		exit(1);
	}

	if(vm_stackpointer == 0){
		puts("VM ERROR");
		puts("Could not find local:");
		puts(name);
		exit(1);
	}
	/*Search for local variables in the vstack...*/

	for(i = vm_stackpointer-1-cur_expr_stack_usage; 
		i >= 0; 
		i--
	){
		if(vm_stack[i].identification == VM_FFRAME_BEGIN) break;
			if(vm_stack[i].vname)
				if(streq(name, vm_stack[i].vname)){
				
					if(vm_stack[i].t.arraylen > 0)
						return vm_stack[i].ldata;
						
					if(vm_stack[i].t.pointerlevel == 0)
						if(vm_stack[i].t.basetype == BASE_STRUCT)
							return vm_stack[i].ldata;
				
					return &(vm_stack[i].smalldata);
				}
		if(i == 0) break;
	}
	/*Search function arguments...*/
	i = 0;
	for(
		j = vm_stackpointer /*points at first empty slot*/
			-1 /*point at the first non-empty slot.*/
			-cur_expr_stack_usage /*skip the expression.*/
			-cur_func_frame_size  /*skip the frame- including local variables.*/
		;
		i < (int64_t)symbol_table[active_function].nargs;
		j=j-1
	)
	{
		if(
			symbol_table[active_function]
				.fargs[i]
				->membername == name
		){
			/*We have it!*/
			/*it's an array, get the ldata*/
			if(vm_stack[j].t.arraylen > 0)
				return vm_stack[j].ldata;
			/*it's a struct, get the ldata*/
			if(vm_stack[j].t.pointerlevel == 0)
				if(vm_stack[j].t.basetype == BASE_STRUCT)
					return vm_stack[j].ldata;
			/*it's a primitive or pointer, get the smalldata.*/
			return &(vm_stack[j].smalldata);
		}
		i++; /*Keep searching the list of args...*/
		j--; /*Go deeper...*/
	}
	puts("VM ERROR");
	puts("Could not find local:");
	puts(name);
	exit(1);
}

/*funky is needed so we can find local variable symbols.*/
void do_expr(expr_node* ee){
	int64_t i = 0;
	int64_t n_subexpressions = 0;
	/*Function calls and method calls both require saving information.*/
	uint64_t saved_cur_func_frame_size = 0;
	uint64_t saved_cur_expr_stack_usage = 0;
	uint64_t saved_active_function=0;
	uint64_t saved_vstack_pointer;
	/*
		TODO:
		* Evaluate all subnodes first.
		* Keep track of how much the stack pointer changes from each node. It should always shift by one- we even keep track of voids.
		* if this is a function call...
			* We have to save the information about our current execution state. Particularly, the i

	*/

	/*Required for the return value.*/
	if(	ee->kind == EXPR_FCALL ||
		ee->kind == EXPR_METHOD ||
		ee->kind == EXPR_BUILTIN_CALL
	){
		uint64_t loc = ast_vm_stack_push_temporary(NULL, ee->t);
		vm_stack[loc].identification = VM_FARGS_BEGIN;
		vm_stack[loc].t = ee->t;
		saved_vstack_pointer = vm_stackpointer;
		for(i = MAX_FARGS; i >= 0; i--){
			if(ee->subnodes[i]){
				do_expr(ee->subnodes[i]);
				n_subexpressions++;
			}
			if(i == 0) break;
		}
	}
	if(
		ee->kind != EXPR_FCALL &&
		ee->kind != EXPR_METHOD &&
		ee->kind != EXPR_BUILTIN_CALL
	){
		saved_vstack_pointer = vm_stackpointer;
		for(i = 0; i < MAX_FARGS; i++){
			if(ee->subnodes[i]){
				do_expr(ee->subnodes[i]);
				n_subexpressions++;
			}
			if(ee->subnodes[i] == NULL)
				break;
		}
	}
	
	/*TODO: remove this if execution is too slow.*/
	if( (vm_stackpointer - saved_vstack_pointer) != (uint64_t)n_subexpressions){
		puts("VM Internal Error");
		puts("Expression evaluation vstack pointer check failed.");
		exit(1);
	}

	if(ee->kind == EXPR_FCALL || ee->kind == EXPR_METHOD){
		//TODO implement function call.

		//Finally ends with popping everything off.
		for(i = 0; i < n_subexpressions; i++) {ast_vm_stack_pop();}
		//pop the last thing off.
		ast_vm_stack_pop();
		return;
	}

	if(ee->kind == EXPR_BUILTIN_CALL){
		if(streq(ee->symname, "__builtin_emit")){
			char* s;
			uint64_t sz;
			//function arguments are backwards on the stack, more arguments = deeper
			memcpy(&s, &vm_stack[vm_stackpointer-1].smalldata, POINTER_SIZE);
			memcpy(&sz, &vm_stack[vm_stackpointer-2].smalldata, POINTER_SIZE);
			impl_builtin_emit(s,sz);
			goto end_of_builtin_call;
		}	
		if(streq(ee->symname, "__builtin_getargc")){
			int32_t v;
			v = impl_builtin_getargc();
			memcpy(&vm_stack[saved_vstack_pointer-1].smalldata, &v, 4);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_getargv")){ //0 arguments
			char** v;
			v = impl_builtin_getargv();
			memcpy(&vm_stack[saved_vstack_pointer-1].smalldata, &v, POINTER_SIZE);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_malloc")){
			char** v;
			uint64_t sz;
			memcpy(&sz, &vm_stack[vm_stackpointer-1].smalldata, 8);
			v = impl_builtin_malloc(sz);
			
			memcpy(&vm_stack[saved_vstack_pointer-1].smalldata, &v, POINTER_SIZE);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_realloc")){
			char* v;
			uint64_t sz;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			memcpy(&sz, &vm_stack[vm_stackpointer-1].smalldata, 8);
			memcpy(&v, &vm_stack[vm_stackpointer-2].smalldata, POINTER_SIZE);
			v = impl_builtin_realloc(v,sz);
			
			memcpy(&vm_stack[saved_vstack_pointer-1].smalldata, &v, POINTER_SIZE);
			goto end_of_builtin_call;
		}		
		if(streq(ee->symname, "__builtin_type_getsz")){
			char* v;
			uint64_t sz;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			
			memcpy(&v, &vm_stack[vm_stackpointer-1].smalldata, POINTER_SIZE);
			sz = impl_builtin_type_getsz(v);
			
			memcpy(&vm_stack[saved_vstack_pointer-1].smalldata, &sz, 8);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_struct_metadata")){
			uint64_t sz;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			
			memcpy(&sz, &vm_stack[vm_stackpointer-1].smalldata, 8);
			sz = impl_builtin_struct_metadata(sz);
			
			memcpy(&vm_stack[saved_vstack_pointer-1].smalldata, &sz, 8);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_strdup")){
			char* v;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			
			memcpy(&v, &vm_stack[vm_stackpointer-1].smalldata, POINTER_SIZE);
			v = impl_builtin_strdup(v);
			
			memcpy(&vm_stack[saved_vstack_pointer-1].smalldata, &v, POINTER_SIZE);
			goto end_of_builtin_call;
		}
		puts("VM Error");
		puts("Unhandled builtin call:");
		puts(ee->symname);
		exit(1);

		end_of_builtin_call:;
		for(i = 0; i < n_subexpressions; i++) {ast_vm_stack_pop();}
		//pop the last thing off.
		ast_vm_stack_pop();
		return;
	}

	
}

