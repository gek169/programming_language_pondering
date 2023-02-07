


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

/*
	used for EXPR_MEMBER
*/
static uint64_t get_offsetof(typedecl* the_struct, char* membername)
{
	uint64_t off = 0;
	uint64_t i;
	for(i = 0; i < the_struct->nmembers; i++){
		if(streq(the_struct->members[i].membername, membername))
			break;
		off = off + type_getsz(the_struct->members[i]);
	}
	return off;
}

static uint64_t do_deref(void* ptr, unsigned sz){
	memcpy(&sz, ptr, sz);
	return sz;
}

/*
	Function to do type conversions. Converts between
	any of the primitives.
*/

static uint64_t do_primitive_type_conversion(
	uint64_t thing,
	uint64_t srcbase, //source base.
	uint64_t whatbase //target base
){
	int8_t i8data;
	uint8_t u8data;
	int16_t i16data;
	uint16_t u16data;
	int32_t i32data;
	uint32_t u32data;
	int64_t i64data;
	uint64_t u64data;
	double ddata;
	float fdata;
	if(sizeof(double) != 8 || sizeof(float) != 4 || sizeof(void*) != 8){
		puts("<Seabass platform error>");
		puts("The interpreter was written with heavy assumption of the sizes of built-in types.");
		puts("Double must be 64 bit, 8 bytes");
		puts("Float must be 32 bit, 4 bytes");
		puts("Pointers must be 8 bytes.");
		puts("You can write a code generator that doesn't respect this,");
		puts("But `codegen` functions must always execute in an environment that abides by these limitations.");
		puts("If your goal was to get a self-hosted implementation on a 32 bit system,");
		puts("You will have to re-write or seriously modify the existing code.");
		puts("Writing a 32bit-to-64bit cross compiler would be even more difficult...");
		exit(1);
	}
	if(srcbase == whatbase) return thing;
	/*integers of the same size are just a reinterpret cast...*/
	if(whatbase == BASE_U8 && srcbase == BASE_I8) return thing;
	if(whatbase == BASE_U16 && srcbase == BASE_I16) return thing;
	if(whatbase == BASE_U32 && srcbase == BASE_I32) return thing;
	if(whatbase == BASE_U64 && srcbase == BASE_I64) return thing;
	if(
		srcbase == BASE_U64 ||
		srcbase == BASE_I64
	){
		i64data = thing;
		u64data = thing;
	}

	if(srcbase == BASE_F64){
		memcpy(&ddata, &thing, 8);
	}
	if(srcbase == BASE_F32){
		memcpy(&fdata, &thing, 4);
	}
	if(srcbase == BASE_U32 || srcbase == BASE_I32){
		memcpy(&u32data, &thing,4);
		memcpy(&i32data, &thing,4);
	}
	if(srcbase == BASE_U16 || srcbase == BASE_I16){
		memcpy(&u16data, &thing,2);
		memcpy(&i16data, &thing,2);
	}
	if(srcbase == BASE_U8 || srcbase == BASE_I8){
		memcpy(&u8data, &thing,1);
		memcpy(&i8data, &thing,1);
	}
	/*
		Somewhat intelligent brute force combinatorics.
		Unsigned to signed never results in a sign extension.
		signed to unsigned is sign extended
		signed to signed is sign extended
	*/
	/*U8 -> X*/
	if(srcbase == BASE_U8){
		if((whatbase == BASE_U16 || whatbase == BASE_I16)){
			u16data = u8data;
			memcpy(&thing, 
			&u16data, 2);
			return thing;
		}
		if((whatbase == BASE_U32 || whatbase == BASE_I32)){
			u32data = u8data;
			memcpy(&thing, 
			&u32data, 4);
			return thing;
		}
		if((whatbase == BASE_U64 || whatbase == BASE_I64)){
			u64data = u8data;
			return u64data;
		}
	}
	/*U16 -> X*/
	if(srcbase == BASE_U16){
		if((whatbase == BASE_U8 || whatbase == BASE_I8)){
			u8data = u16data;
			memcpy(&thing, 
			&u8data, 1);
			return thing;
		}
		if((whatbase == BASE_U32 || whatbase == BASE_I32)){
			u32data = u16data;
			memcpy(&thing, 
			&u32data, 4);
			return thing;
		}
		if((whatbase == BASE_U64 || whatbase == BASE_I64)){
			u64data = u16data;
			return u64data;
		}
	}
	/*U32 -> X*/
	if(srcbase == BASE_U32){
		if((whatbase == BASE_U8 || whatbase == BASE_I8)){
			u8data = u32data;
			memcpy(&thing, 
			&u8data, 1);
			return thing;
		}
		if((whatbase == BASE_U16 || whatbase == BASE_I16)){
			u16data = u32data;
			memcpy(&thing, 
			&u16data, 2);
			return thing;
		}
		if((whatbase == BASE_U64 || whatbase == BASE_I64)){
			u64data = u32data;
			return u64data;
		}
	}
	/*U64 -> X*/
	if(srcbase == BASE_U64){
		if((whatbase == BASE_U8 || whatbase == BASE_I8)){
			u8data = u64data;
			memcpy(&thing, 
			&u8data, 1);
			return thing;
		}
		if((whatbase == BASE_U16 || whatbase == BASE_I16)){
			u16data = u64data;
			memcpy(&thing, 
			&u16data, 2);
			return thing;
		}
		if((whatbase == BASE_U32 || whatbase == BASE_I32)){
			u32data = u64data;
			memcpy(&thing, 
			&u32data, 4);
			return thing;
		}
	}
	/*i8 -> x*/
	if(srcbase == BASE_I8){
		if((whatbase == BASE_U16 || whatbase == BASE_I16)){
			i16data = i8data;
			memcpy(&thing, 
			&i16data, 2);
			return thing;

		}
		if((whatbase == BASE_U32 || whatbase == BASE_I32)){
			i32data = i8data;
			memcpy(&thing, 
			&i32data, 4);
			return thing;

		}
		if((whatbase == BASE_U64 || whatbase == BASE_I64)){
			i64data = i8data;
			return i64data;
		}
	}
	/*i16 -> x*/
	if(srcbase == BASE_I16){
		if((whatbase == BASE_U8 || whatbase == BASE_I8)){
			i8data = i16data;
			memcpy(&thing, 
			&i8data, 1);
			return thing;
		}
		if((whatbase == BASE_U32 || whatbase == BASE_I32)){
			i32data = i16data;
			memcpy(&thing, 
			&i32data, 4);
			return thing;
		}
		if((whatbase == BASE_U64 || whatbase == BASE_I64)){
			i64data = i16data;
			return i64data;
		}
	}
	/*i32 -> x*/
	if(srcbase == BASE_I32){
		if((whatbase == BASE_U8 || whatbase == BASE_I8)){
			i8data = i32data;
			memcpy(&thing, 
			&i8data, 1);
			return thing;
		}
		if((whatbase == BASE_U16 || whatbase == BASE_I16)){
			i16data = i32data;
			memcpy(&thing, 
			&i16data, 2);
			return thing;
		}
		if((whatbase == BASE_U64 || whatbase == BASE_I64)){
			i64data = i32data;
			return i64data;
		}
	}
	/*i64 -> x*/
	if(srcbase == BASE_I64){
		if((whatbase == BASE_U8 || whatbase == BASE_I8)){
			i8data = i64data;
			memcpy(&thing, 
			&i8data, 1);
			return thing;
		}
		if((whatbase == BASE_U16 || whatbase == BASE_I16)){
			i16data = i64data;
			memcpy(&thing, 
			&i16data, 2);
			return thing;
		}
		if((whatbase == BASE_U32 || whatbase == BASE_I32)){
			i32data = i64data;
			memcpy(&thing, 
			&i32data, 4);
			return thing;
		}
	}

	/*
		Float-to-float conversions!
	*/
	if(srcbase == BASE_F32 && whatbase == BASE_F64){
		ddata = fdata;
		memcpy(&thing, 
		&ddata, 8);
		return thing;
	}
	if(srcbase == BASE_F64 && whatbase == BASE_F32){
		fdata = ddata;
		memcpy(&thing, 
		&fdata, 4);
		return thing;
	}
	/*
		Floating point to integer conversions!
	*/

	//prepare by setting up i64data for copying...
	if(srcbase == BASE_F64){
		i64data = ddata;
	}
	if(srcbase == BASE_F32){
		i64data = fdata;
	}
	//do the copy!
	if(whatbase == BASE_U8 || whatbase == BASE_I8){
		u8data = i64data;
		memcpy(&thing, 
		&u8data,1);
		return thing;
	}
	if(whatbase == BASE_U16 || whatbase == BASE_I16){
		u16data = i64data;
		memcpy(&thing, 
		&u16data,2);
		return thing;
	}
	if(whatbase == BASE_U32 || whatbase == BASE_I32){
		u32data = i64data;
		memcpy(&thing, &u32data,4);
		return thing;
	}
	//it was one of the 64 bit ones. In which case, i64data already holds the proper result.
	return i64data;
}

/*a
static float do_fmod(float a, float b){return a % b;}
static double do_dmod(double a, double b){return a % b;}
*/


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
			if(vm_stack[i].identification == VM_VARIABLE)
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
		i < (int64_t)symbol_table[active_function].nargs;//MORE ARGS
		j=j-1 											 //DEEPER
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

	/*This one is only used for sanity checking.*/
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
	/*Otherwise, in-order*/
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

	if(ee->kind == EXPR_SIZEOF ||
		ee->kind == EXPR_INTLIT ||
		ee->kind == EXPR_CONSTEXPR_INT
	){
		uint64_t general;
		//push a temporary.
		general = ast_vm_stack_push_temporary(NULL, ee->t);
		vm_stack[general].smalldata = ee->idata;
		return;
	}

	if(ee->kind == EXPR_FLOATLIT ||
		ee->kind == EXPR_CONSTEXPR_FLOAT
	){
		uint64_t general;
		//push a temporary.
		general = ast_vm_stack_push_temporary(NULL, ee->t);
		memcpy(&vm_stack[general].smalldata,&ee->fdata,8);
		return;
	}

	if(ee->kind == EXPR_FCALL || ee->kind == EXPR_METHOD){
		//TODO implement function call.
		saved_cur_func_frame_size = cur_func_frame_size;
		saved_cur_expr_stack_usage = cur_expr_stack_usage;
		saved_active_function= active_function;
		saved_vstack_pointer = vm_stackpointer;

		/*TODO: invoke ast_call_function*/
			puts("VM ERROR- expression fcall/method is NYI");
			exit(1);
		
		cur_func_frame_size = saved_cur_func_frame_size;
		cur_expr_stack_usage = saved_cur_expr_stack_usage;
		active_function = saved_active_function;
		vm_stackpointer = saved_vstack_pointer;
		//Finally ends with popping everything off.
		for(i = 0; i < n_subexpressions; i++) {ast_vm_stack_pop();}
		//Don't! pop the last thing off because THAT'S WHERE THE RETURN VALUE IS!
		//ast_vm_stack_pop();
		return;
	}
	if(ee->kind == EXPR_LSYM){
		void* p;		
		uint64_t general;
		p = retrieve_variable_memory_pointer(ee->symname, 0);

		general = ast_vm_stack_push_temporary(NULL, ee->t);
		memcpy(&vm_stack[general].smalldata,&p,8);
		return;
	}
	if(ee->kind == EXPR_GSYM){
		void* p;		
		uint64_t general;
		p = retrieve_variable_memory_pointer(ee->symname, 1);
		general = ast_vm_stack_push_temporary(NULL, ee->t);
		memcpy(&vm_stack[general].smalldata,&p,8);
		return;
	}
	if(ee->kind == EXPR_STRINGLIT){
		void* p;
		uint64_t general;
		p = symbol_table[ee->symid].cdata;

		general = ast_vm_stack_push_temporary(NULL, ee->t);
		memcpy(&vm_stack[general].smalldata,&p,8);
		return;
	}
	if(ee->kind == EXPR_MEMBER){ /*the actual operation is to get a pointer to the member.*/
		void* p;
		uint64_t pt;
		uint64_t levels_of_indirection;
		pt = vm_stack[vm_stackpointer-1].smalldata;
		levels_of_indirection = ee->subnodes[0]->t.pointerlevel;
		/*
			You can access the members of a struct with arbitrary levels of indirection.
			and if it is more than one, then we need to dereference a pointer-to-pointer.
		*/
		while(levels_of_indirection > 1){
			memcpy(&p, &pt, POINTER_SIZE);
			pt = do_deref(p, POINTER_SIZE);
			levels_of_indirection = levels_of_indirection - 1;
		}
		pt += get_offsetof(type_table + ee->subnodes[0]->t.structid, ee->symname);
		vm_stack[vm_stackpointer-1].smalldata = pt;
		vm_stack[vm_stackpointer-1].t = ee->t;
		return;
	}
	if(ee->kind == EXPR_CAST){
		uint64_t data;
		void* p;
		data = vm_stack[vm_stackpointer-1].smalldata;
		memcpy(&p, &data, POINTER_SIZE);
		/*if is lvalue, then do a dereference...*/
		if(vm_stack[vm_stackpointer-1].t.is_lvalue){
			data = do_deref((void*)data, type_getsz(ee->t));
		}
		/*If they were both pointers, we do nothing.*/
		if(ee->t.pointerlevel > 0)
			if(vm_stack[vm_stackpointer-1].t.pointerlevel > 0)
				goto end_expr_cast;
		/*if it was integer to pointer, 
			we're converting from U64 to whatever the integer is.
			Note that it's impossible for it to be a float, here.
			that should have already been caught by the validator.
		*/
		if(ee->t.pointerlevel){
			data = do_primitive_type_conversion(
				data,
				vm_stack[vm_stackpointer-1].t.basetype,
				BASE_U64
			);
			vm_stack[vm_stackpointer-1].smalldata = data;
			goto end_expr_cast;
		}
		/*
			If it's pointer to integer, it's the opposite.
		*/
		if(vm_stack[vm_stackpointer-1].t.pointerlevel){
			data = do_primitive_type_conversion(
				data,
				BASE_U64,
				ee->t.basetype
			);
			vm_stack[vm_stackpointer-1].smalldata = data;
			goto end_expr_cast;
		}
		/*It doesn't involve pointers. Neat! We can just do it right here, then:*/
		data = do_primitive_type_conversion(
			data,
			vm_stack[vm_stackpointer-1].t.basetype,
			ee->t.basetype
		);
		vm_stack[vm_stackpointer-1].smalldata = data;
		end_expr_cast:;
		vm_stack[vm_stackpointer-1].t = ee->t;
		return;
	}
	if(ee->kind == EXPR_INDEX){
		uint64_t data;
		int64_t ind;
		void* p;
		ind = vm_stack[vm_stackpointer-2].smalldata;
		/*It will never be an lvalue!*/
		memcpy(&p, &data, POINTER_SIZE);
		
		//perform the pointer addition
			p = do_ptradd(p, ind, type_getsz(ee->t));
		memcpy( &vm_stack[vm_stackpointer-2].smalldata, &p, POINTER_SIZE);
		vm_stack[vm_stackpointer-2].t = ee->t;
		ast_vm_stack_pop(); //we no longer need the index itself.
		return;
	}
	if(ee->kind == EXPR_ADD){
		int32_t i32data;
		uint32_t u32data;		
		int16_t i16data;
		uint16_t u16data;		
		int8_t i8data;
		uint8_t u8data;
		float f32_1;
		float f32_2;
		double f64_1;
		double f64_2;
		/*case 1: both are i64 or u64, or pointer arithmetic.*/
		if(
			vm_stack[vm_stackpointer-2].t.pointerlevel > 0 ||
			vm_stack[vm_stackpointer-1].t.pointerlevel > 0 ||
			vm_stack[vm_stackpointer-1].t.basetype == BASE_U64 ||
			vm_stack[vm_stackpointer-1].t.basetype == BASE_I64
		){
			vm_stack[vm_stackpointer-2].smalldata = 
				vm_stack[vm_stackpointer-1].smalldata + 
				vm_stack[vm_stackpointer-2].smalldata
			;
			goto end_expr_add;
		}
		/*case 3: both are i32 or u32*/
		if(
			vm_stack[vm_stackpointer-1].t.basetype == BASE_U32 ||
			vm_stack[vm_stackpointer-1].t.basetype == BASE_I32
		){
			memcpy(&i32data, &vm_stack[vm_stackpointer-1].smalldata, 4);
			memcpy(&u32data, &vm_stack[vm_stackpointer-2].smalldata, 4);
			i32data = i32data + u32data;
			memcpy(&vm_stack[vm_stackpointer-2].smalldata, &i32data, 4);
			goto end_expr_add;
		}
		/*case 4: both are i16 or u16*/
		if(
			vm_stack[vm_stackpointer-1].t.basetype == BASE_U16 ||
			vm_stack[vm_stackpointer-1].t.basetype == BASE_I16
		){
			memcpy(&i16data, &vm_stack[vm_stackpointer-1].smalldata, 2);
			memcpy(&u16data, &vm_stack[vm_stackpointer-2].smalldata, 2);
			i16data = i16data + u16data;
			memcpy(&vm_stack[vm_stackpointer-2].smalldata, &i16data, 2);
			goto end_expr_add;
		}	
		/*case 5: both are i8 or u8*/
		if(
			vm_stack[vm_stackpointer-1].t.basetype == BASE_U8 ||
			vm_stack[vm_stackpointer-1].t.basetype == BASE_I8
		){
			memcpy(&i8data, &vm_stack[vm_stackpointer-1].smalldata, 1);
			memcpy(&u8data, &vm_stack[vm_stackpointer-2].smalldata, 1);
			i8data = i8data + u8data;
			memcpy(&vm_stack[vm_stackpointer-2].smalldata, &i8data, 1);
			goto end_expr_add;
		}
		/*Case 6: both are f32*/
		if(
			vm_stack[vm_stackpointer-1].t.basetype == BASE_F32
		){
			memcpy(&f32_1, &vm_stack[vm_stackpointer-1].smalldata, 4);
			memcpy(&f32_2, &vm_stack[vm_stackpointer-2].smalldata, 4);
			f32_1 = f32_1 + f32_2;
			memcpy(&vm_stack[vm_stackpointer-2].smalldata, &f32_1, 4);
			goto end_expr_add;
		}		/*Case 6: both are f64*/
		if(
			vm_stack[vm_stackpointer-1].t.basetype == BASE_F64
		){
			memcpy(&f64_1, &vm_stack[vm_stackpointer-1].smalldata, 8);
			memcpy(&f64_2, &vm_stack[vm_stackpointer-2].smalldata, 8);
			f64_1 = f64_1 + f64_2;
			memcpy(&vm_stack[vm_stackpointer-2].smalldata, &f64_1, 8);
			goto end_expr_add;
		}
		puts("VM ERROR");
		puts("Unhandled add type.");
		exit(1);

		end_expr_add:;
		vm_stack[vm_stackpointer-2].t = ee->t;
		ast_vm_stack_pop(); //we no longer need the index itself.
		return;
	}
	if(ee->kind == EXPR_SUB){
		int32_t i32data;
		uint32_t u32data;		
		int16_t i16data;
		uint16_t u16data;		
		int8_t i8data;
		uint8_t u8data;
		float f32_1;
		float f32_2;
		double f64_1;
		double f64_2;
		/*case 1: both are i64 or u64, or pointer arithmetic.*/
		if(
			vm_stack[vm_stackpointer-1].t.pointerlevel > 0 ||
			vm_stack[vm_stackpointer-1].t.basetype == BASE_U64 ||
			vm_stack[vm_stackpointer-1].t.basetype == BASE_I64
		){
			vm_stack[vm_stackpointer-2].smalldata = 
				vm_stack[vm_stackpointer-1].smalldata -
				vm_stack[vm_stackpointer-2].smalldata
			;
			goto end_expr_sub;
		}
		/*case 3: both are i32 or u32*/
		if(
			vm_stack[vm_stackpointer-1].t.basetype == BASE_U32 ||
			vm_stack[vm_stackpointer-1].t.basetype == BASE_I32
		){
			memcpy(&i32data, &vm_stack[vm_stackpointer-1].smalldata, 4);
			memcpy(&u32data, &vm_stack[vm_stackpointer-2].smalldata, 4);
			i32data = i32data - u32data;
			memcpy(&vm_stack[vm_stackpointer-2].smalldata, &i32data, 4);
			goto end_expr_sub;
		}
		/*case 4: both are i16 or u16*/
		if(
			vm_stack[vm_stackpointer-1].t.basetype == BASE_U16 ||
			vm_stack[vm_stackpointer-1].t.basetype == BASE_I16
		){
			memcpy(&i16data, &vm_stack[vm_stackpointer-1].smalldata, 2);
			memcpy(&u16data, &vm_stack[vm_stackpointer-2].smalldata, 2);
			i16data = i16data - u16data;
			memcpy(&vm_stack[vm_stackpointer-2].smalldata, &i16data, 2);
			goto end_expr_sub;
		}	
		/*case 5: both are i8 or u8*/
		if(
			vm_stack[vm_stackpointer-1].t.basetype == BASE_U8 ||
			vm_stack[vm_stackpointer-1].t.basetype == BASE_I8
		){
			memcpy(&i8data, &vm_stack[vm_stackpointer-1].smalldata, 1);
			memcpy(&u8data, &vm_stack[vm_stackpointer-2].smalldata, 1);
			i8data = i8data - u8data;
			memcpy(&vm_stack[vm_stackpointer-2].smalldata, &i8data, 1);
			goto end_expr_sub;
		}
		/*Case 6: both are f32*/
		if(
			vm_stack[vm_stackpointer-1].t.basetype == BASE_F32
		){
			memcpy(&f32_1, &vm_stack[vm_stackpointer-1].smalldata, 4);
			memcpy(&f32_2, &vm_stack[vm_stackpointer-2].smalldata, 4);
			f32_1 = f32_1 - f32_2;
			memcpy(&vm_stack[vm_stackpointer-2].smalldata, &f32_1, 4);
			goto end_expr_sub;
		}	
		/*Case 7: both are f64*/
		if(
			vm_stack[vm_stackpointer-1].t.basetype == BASE_F64
		){
			memcpy(&f64_1, &vm_stack[vm_stackpointer-1].smalldata, 8);
			memcpy(&f64_2, &vm_stack[vm_stackpointer-2].smalldata, 8);
			f64_1 = f64_1 - f64_2;
			memcpy(&vm_stack[vm_stackpointer-2].smalldata, &f64_1, 8);
			goto end_expr_sub;
		}
		puts("VM ERROR");
		puts("Unhandled sub type.");
		exit(1);

		end_expr_sub:;
		vm_stack[vm_stackpointer-2].t = ee->t;
		ast_vm_stack_pop(); //we no longer need the index itself.
		return;
	}



	/*
		Builtin calls, basically the hook that the codegen code gets into the compiletime environment.
	*/
	
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
			memcpy(&v, &vm_stack[vm_stackpointer-1].smalldata, POINTER_SIZE);
			memcpy(&sz, &vm_stack[vm_stackpointer-2].smalldata, 8);
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
		if(streq(ee->symname, "__builtin_free")){
			char* v;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			
			memcpy(&v, &vm_stack[vm_stackpointer-1].smalldata, POINTER_SIZE);
			impl_builtin_free(v);
			
			//memcpy(&vm_stack[saved_vstack_pointer-1].smalldata, &v, POINTER_SIZE);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_exit")){
			int32_t v;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			
			memcpy(&v, &vm_stack[vm_stackpointer-1].smalldata, 4);
			impl_builtin_exit(v);
			
			//memcpy(&vm_stack[saved_vstack_pointer-1].smalldata, &v, POINTER_SIZE);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_get_ast")){
			char* v;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			
			//memcpy(&v, &vm_stack[vm_stackpointer-1].smalldata, 4);
			v = (char*) impl_builtin_get_ast();
			
			memcpy(&vm_stack[saved_vstack_pointer-1].smalldata, &v, POINTER_SIZE);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_peek")){
			char* v;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			
			//memcpy(&v, &vm_stack[vm_stackpointer-1].smalldata, 4);
			v = (char*) impl_builtin_peek();
			
			memcpy(&vm_stack[saved_vstack_pointer-1].smalldata, &v, POINTER_SIZE);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_consume")){
			char* v;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			
			//memcpy(&v, &vm_stack[vm_stackpointer-1].smalldata, 4);
			v = (char*) impl_builtin_consume();
			
			memcpy(&vm_stack[saved_vstack_pointer-1].smalldata, &v, POINTER_SIZE);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_puts")){
			char* v;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			
			memcpy(&v, &vm_stack[vm_stackpointer-1].smalldata, POINTER_SIZE);
			impl_builtin_puts(v);
			
			//memcpy(&vm_stack[saved_vstack_pointer-1].smalldata, &v, POINTER_SIZE);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_gets")){
			char* v;
			uint64_t sz;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			
			memcpy(&v, &vm_stack[vm_stackpointer-1].smalldata, POINTER_SIZE);
			memcpy(&sz, &vm_stack[vm_stackpointer-2].smalldata, 8);
			impl_builtin_gets(v,sz);
			
			//memcpy(&vm_stack[saved_vstack_pointer-1].smalldata, &v, POINTER_SIZE);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_open_ofile")){
			char* v;
			int32_t q;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			
			memcpy(&v, &vm_stack[vm_stackpointer-1].smalldata, POINTER_SIZE);
			//memcpy(&q, &vm_stack[vm_stackpointer-2].smalldata, 4);
			q = impl_builtin_open_ofile(v);
			memcpy(&vm_stack[saved_vstack_pointer-1].smalldata, &q, 4);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_close_ofile")){
			impl_builtin_close_ofile();
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_validate_function")){
			char* v;
			memcpy(&v, &vm_stack[vm_stackpointer-1].smalldata, POINTER_SIZE);
			impl_builtin_validate_function(v);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_validate_function")){
			char* a;
			char* b;
			uint64_t c;
			memcpy(&a, &vm_stack[vm_stackpointer-1].smalldata, POINTER_SIZE);
			memcpy(&b, &vm_stack[vm_stackpointer-1].smalldata, POINTER_SIZE);
			memcpy(&c, &vm_stack[vm_stackpointer-1].smalldata, 8);
			impl_builtin_memcpy(a,b,c);
			goto end_of_builtin_call;
		}
		puts("VM Error");
		puts("Unhandled builtin call:");
		puts(ee->symname);
		exit(1);
		end_of_builtin_call:;
		for(i = 0; i < n_subexpressions; i++) {ast_vm_stack_pop();}
		//DON'T pop the last thing off, it's the return value!
		//ast_vm_stack_pop();
		return;
	}

	
}

