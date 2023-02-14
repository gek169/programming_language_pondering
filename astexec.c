


#include "stringutil.h"
#include "targspecifics.h"

#include "libmin.h"
#include "parser.h"
#include "data.h"
#include "metaprogramming.h"
#include "astexec.h"


static uint64_t is_debugging = 1;

static void debug_print(char* msg, uint64_t printme1, uint64_t printme2){
	if(is_debugging == 0) return;
	char buf[100];
	puts(msg);

	if(printme1){
		mutoa(buf, printme1);
		puts(buf);
	}
	if(printme2){
		mutoa(buf, printme2);
		puts(buf);
	}
}


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
	uint64_t placement;
	if(cur_expr_stack_usage){
		puts("VM error");
		puts("Tried to push local variable in the middle of an expression? Huh?");
	}

	placement = ast_vm_stack_push();
	cur_func_frame_size++;
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
		//debug_print("Allocating a struct!",0,0);
		vm_stack[placement].ldata = calloc(type_getsz(s->t),1);
	}
	return placement;
}

static uint64_t ast_vm_stack_push_temporary(char* name, type t){
	uint64_t placement = ast_vm_stack_push();
	vm_stack[placement].vname = name;
	vm_stack[placement].identification = VM_EXPRESSION_TEMPORARY;
	vm_stack[placement].ldata = NULL; //forbidden to have a struct or 
	if(t.arraylen){
		t.pointerlevel++;
		t.arraylen = 0;
		t.is_lvalue  = 0;
	}
	if(t.arraylen == 0)
	if(t.pointerlevel == 0)
	if(t.basetype == BASE_STRUCT){
		{
			puts("<VM ERROR>");
			puts("Struct temporary?");
			exit(1);
		}
	}
	vm_stack[placement].t = t;
	cur_expr_stack_usage++;
	return placement;
}

static void ast_vm_stack_pop(){
	if(vm_stackpointer == 0){
		puts("<VM ERROR> stack was popped while empty.");
		exit(1);
	}
	if(vm_stack[vm_stackpointer-1].identification == VM_EXPRESSION_TEMPORARY) cur_expr_stack_usage--;
	if(vm_stack[vm_stackpointer-1].identification == VM_VARIABLE) cur_func_frame_size--;
	if(vm_stack[vm_stackpointer-1].ldata) free(vm_stack[vm_stackpointer-1].ldata);
	vm_stack[vm_stackpointer-1].ldata = NULL;
	vm_stack[vm_stackpointer-1].vname = NULL; /*Don't want to accidentally pick this up in the future!*/
	vm_stackpointer--;
}

static void ast_vm_stack_pop_temporaries(){
	while(cur_expr_stack_usage > 0) ast_vm_stack_pop();
}

static void ast_vm_stack_pop_lvars(){
	while(cur_func_frame_size > 0) ast_vm_stack_pop();
}

static void* do_ptradd(char* ptr, int64_t num, uint64_t sz){
	return ptr + num * sz;
}

/*
	used for EXPR_MEMBER
*/
static uint64_t get_offsetof(
	typedecl* the_struct, 
	char* membername
)
{
	uint64_t off = 0;
	uint64_t i;
	for(i = 0; i < the_struct->nmembers; i++){
		if(streq(the_struct->members[i].membername, membername))
			return off;
		off = off + type_getsz(the_struct->members[i]);
	}
	puts("VM internal error");
	puts("Could not find member of struct:");
	puts(the_struct->name);
	puts("member:");
	puts(membername);
	exit(1);
	return off;
}

uint64_t vm_get_offsetof(
	typedecl* the_struct, 
	char* membername
){
	return get_offsetof(the_struct, membername);
}

static uint64_t do_deref(void* ptr, unsigned sz){
	if(sz > 8){
		puts("VM Internal error");
		puts("VM was asked to dereference more than 8 bytes...");
		exit(1);
	}
	memcpy(
		&sz, 
		ptr, 
		sz
	);
	return sz;
}

static void do_assignment(
	void* ptr,
	uint64_t val,
	uint64_t sz
){
	memcpy(ptr, &val, sz);
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
	double f64data;
	float f32data;
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
	if(srcbase == BASE_VOID ||
		whatbase == BASE_VOID ||
		srcbase > BASE_F64 ||
		whatbase > BASE_F64 ){
		puts("VM Error:");
		puts("Tried to do conversions between INVALID types!");
		if(srcbase == BASE_STRUCT) puts("srcbase was BASE_STRUCT");
		if(whatbase == BASE_STRUCT) puts("whatbase was BASE_STRUCT");
		exit(1);		
	}
	/*integers of the same size are just a reinterpret cast...*/
	if(whatbase == BASE_U8 && srcbase == BASE_I8) return thing;
	if(whatbase == BASE_U16 && srcbase == BASE_I16) return thing;
	if(whatbase == BASE_U32 && srcbase == BASE_I32) return thing;
	if(whatbase == BASE_U64 && srcbase == BASE_I64) return thing;
	/*Generate all possible input interpretations.*/
	i64data = thing;
	u64data = thing;
	memcpy(&f64data, &thing, 8);
	memcpy(&f32data, &thing, 4);
	memcpy(&u32data, &thing,4);
	i32data = u32data;
	memcpy(&u16data, &thing,2);
	i16data = u16data;
	memcpy(&u8data, &thing,1);
	i8data = u8data;
	if(whatbase < srcbase){
		if(whatbase == BASE_U8 ||whatbase == BASE_I8){
			if(srcbase == BASE_U16 || srcbase == BASE_I16)	u8data=u16data;
			if(srcbase == BASE_U32 || srcbase == BASE_I32)	u8data=u32data;
			if(srcbase == BASE_U64 || srcbase == BASE_I64)	u8data=u64data;
			if(srcbase == BASE_F32)							{i8data=f32data;u8data=i8data;}
			if(srcbase == BASE_F64)							{i8data=f64data;u8data=i8data;}
			memcpy(&thing, &u8data, 1);
			return thing;
		}
		if(whatbase == BASE_U16 ||whatbase == BASE_I16){
			if(srcbase == BASE_U32 || srcbase == BASE_I32)	u16data=u32data;
			if(srcbase == BASE_U64 || srcbase == BASE_I64)	u16data=u64data;
			if(srcbase == BASE_F32)							{i16data=f32data;u16data=i16data;}
			if(srcbase == BASE_F64)							{i16data=f64data;u16data=i16data;}
			memcpy(&thing, &u16data, 2);
			return thing;
		}
		if(whatbase == BASE_U32 ||whatbase == BASE_I32){
			if(srcbase == BASE_U64 || srcbase == BASE_I64)	u32data=u64data;
			if(srcbase == BASE_F32)							{i32data=f32data;u32data=i32data;}
			if(srcbase == BASE_F64)							{i32data=f64data;u32data=i32data;}
			memcpy(&thing, &u32data, 4);
			return thing;
		}
		if(whatbase == BASE_U64 ||whatbase == BASE_I64){
			if(srcbase == BASE_F32)							{i64data=f32data;u64data=i64data;}
			if(srcbase == BASE_F64)							{i64data=f64data;u64data=i64data;}
			//memcpy(&thing, &u64data, 8);
			return u64data;
		}
		if(whatbase == BASE_F32){
			if(srcbase == BASE_F64)							f32data=f64data;
			memcpy(&thing, &f32data, 4);
			return thing;
		}
		//base_f64 is equal to itself						f64data=f64data;
	}
	//Promoting up?
	if(whatbase == BASE_F64){
		if(srcbase == BASE_I8) 								f64data = i8data;
		if(srcbase == BASE_U8) 								f64data = u8data;
		
		if(srcbase == BASE_I16) 							f64data = i16data;
		if(srcbase == BASE_U16) 							f64data = u16data;
		
		if(srcbase == BASE_I32) 							f64data = i32data;
		if(srcbase == BASE_U32) 							f64data = u32data;
		
		if(srcbase == BASE_I64) 							f64data = i64data;
		if(srcbase == BASE_U64) 							f64data = u64data;
				
		if(srcbase == BASE_I64) 							f64data = i64data;
		if(srcbase == BASE_U64) 							f64data = u64data;
		
		if(srcbase == BASE_F32) 							f64data = f32data;
		memcpy(&thing, &f64data, 8);
		return thing;
	}	
	if(whatbase == BASE_F32){
		if(srcbase == BASE_I8) 								f32data = i8data;
		if(srcbase == BASE_U8) 								f32data = u8data;
		
		if(srcbase == BASE_I16) 							f32data = i16data;
		if(srcbase == BASE_U16) 							f32data = u16data;
		
		if(srcbase == BASE_I32) 							f32data = i32data;
		if(srcbase == BASE_U32) 							f32data = u32data;
		
		if(srcbase == BASE_I64) 							f32data = i64data;
		if(srcbase == BASE_U64) 							f32data = u64data;
				
		if(srcbase == BASE_U64) 							f32data = u64data;
		if(srcbase == BASE_I64) 							f32data = i64data;
		memcpy(&thing, &f32data, 4);
		return thing;
	}
	if(whatbase == BASE_I64){
		if(srcbase == BASE_I8) 								i64data = i8data;
		if(srcbase == BASE_U8) 								i64data = u8data;
		
		if(srcbase == BASE_I16) 							i64data = i16data;
		if(srcbase == BASE_U16) 							i64data = u16data;
		
		if(srcbase == BASE_I32) 							i64data = i32data;
		if(srcbase == BASE_U32) 							i64data = u32data;
		//memcpy(&thing, &f32data, 4);
		return i64data;
	}
	if(whatbase == BASE_U64){
		if(srcbase == BASE_I8) 								{i64data = i8data;u64data=i64data;}//must enforce sign extension.
		if(srcbase == BASE_U8) 								u64data = u8data;
		
		if(srcbase == BASE_I16) 							{i64data = i16data;u64data=i64data;}//must enforce sign extension.
		if(srcbase == BASE_U16) 							u64data = u16data;
		
		if(srcbase == BASE_I32) 							{i64data = i32data;u64data=i64data;}//must enforce sign extension.
		if(srcbase == BASE_U32) 							u64data = u32data;
				
		//memcpy(&thing, &u64data, 8);
		return u64data;
	}
	if(whatbase == BASE_I32){
		if(srcbase == BASE_U8) 								i32data = u8data;
		if(srcbase == BASE_I8) 								i32data = i8data;//must enforce sign extension.
		
		if(srcbase == BASE_U16) 							i32data = u16data;
		if(srcbase == BASE_I16) 							i32data = i16data;//must enforce sign extension.
		memcpy(&thing, &i32data, 4);
		return thing;
	}
	if(whatbase == BASE_U32){
		if(srcbase == BASE_U8) 								u32data = u8data;
		if(srcbase == BASE_I8) 								{i32data = i8data;u32data =i32data;}//must enforce sign extension.
		
		if(srcbase == BASE_U16) 							u32data = u16data;
		if(srcbase == BASE_I16) 							{i32data = i16data;u32data =i32data;}//must enforce sign extension.
		memcpy(&thing, &u32data, 4);
		return thing;
	}
	if(whatbase == BASE_I16){
		if(srcbase == BASE_U8) 								i16data = u8data;
		if(srcbase == BASE_I8) 								i16data = i8data;//must enforce sign extension.
		memcpy(&thing, &i16data, 2);
		return thing;
	}
	if(whatbase == BASE_U16){
		if(srcbase == BASE_U8) 								u16data = u8data;
		if(srcbase == BASE_I8) 								{i16data = i8data;u16data=i16data;}//must enforce sign extension.
		memcpy(&thing, &u16data, 2);
		return thing;
	}
	puts("VM internal error");
	puts("Unhandled type conversion.");
	if(srcbase == BASE_U8) puts("from u8");
	if(srcbase == BASE_I8) puts("from i8");	
	if(srcbase == BASE_U16) puts("from u16");
	if(srcbase == BASE_I16) puts("from i16");
	if(srcbase == BASE_U32) puts("from u32");
	if(srcbase == BASE_I32) puts("from i32");
	if(srcbase == BASE_U64) puts("from u64");
	if(srcbase == BASE_I64) puts("from i64");
	if(srcbase == BASE_F32) puts("from f32");
	if(srcbase == BASE_F64) puts("from f64");
	if(srcbase == BASE_VOID) puts("from void");
	if(srcbase >= BASE_FUNCTION) puts("from invalid");	

	if(whatbase == BASE_U8) puts("to u8");
	if(whatbase == BASE_I8) puts("to i8");	
	if(whatbase == BASE_U16) puts("to u16");
	if(whatbase == BASE_I16) puts("to i16");
	if(whatbase == BASE_U32) puts("to u32");
	if(whatbase == BASE_I32) puts("to i32");
	if(whatbase == BASE_U64) puts("to u64");
	if(whatbase == BASE_I64) puts("to i64");
	if(whatbase == BASE_F32) puts("to f32");
	if(whatbase == BASE_F64) puts("to f64");
	if(whatbase == BASE_VOID) puts("to void");
	if(whatbase >= BASE_FUNCTION) puts("to invalid");
	exit(1);
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
//				debug_print("\nHaving to allocate global storage...",0,0);
				symbol_table[i].cdata = calloc(
					sz,1
				);
				if(symbol_table[i].cdata == NULL){
					puts("\nfailed trying to calloc a global variable's cdata.");
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
		puts("Cannot get a local when the vm stack is empty.");
		puts(name);
		exit(1);
	}
	/*Search for local variables in the vstack...*/

	for(i = vm_stackpointer-1-cur_expr_stack_usage; 
		i > (
			(int64_t)vm_stackpointer //points to first empty slot
				-(int64_t)1 //point to the first non-empty slot instead.
				-(int64_t)cur_expr_stack_usage //how many expression temporaries are there?
				-(int64_t)cur_func_frame_size //how many variables are there in the frame?
		); 
		i--
	){
		if(vm_stack[i].identification == VM_FFRAME_BEGIN) break;
		
		if(vm_stack[i].identification == VM_VARIABLE)
			if(vm_stack[i].vname)
				if(streq(name, vm_stack[i].vname)){
				
					if(vm_stack[i].t.arraylen > 0)
						return vm_stack[i].ldata;
						
					if(vm_stack[i].t.pointerlevel == 0)
						if(vm_stack[i].t.basetype == BASE_STRUCT){
							return vm_stack[i].ldata;
						}
					return &(vm_stack[i].smalldata);
				}
		if(i == 0) break;
	}
	/*Search function arguments...*/
	i = 0;
	for(
		j = (int64_t)vm_stackpointer 		/*points at first empty slot*/
			-(int64_t)1 					/*point at the first non-empty slot.*/
			-(int64_t)cur_expr_stack_usage /*skip the expression.*/
			-(int64_t)cur_func_frame_size  /*skip the frame- including local variables.*/
		;
		i < (int64_t)symbol_table[active_function].nargs;//i is tracking which argument we're checking.
	)
	{
		//debug_print("Searching Arguments Iteration:",i+1,0);
		if(
			streq(
			symbol_table[active_function]
				.fargs[i]
				->membername,
			name
			)
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
		j--;
	}
	puts("VM ERROR");
	puts("Could not find local:");
	puts(name);
	exit(1);
}

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

	/*Do the subnodes in reverse order!
		The result is that the arguments are placed on the stack in the reverse order,
		which is the VM's ABI.

		fn myFunc(int a1, int a2, int a3):
			int a4;
			int a5;
		end
		becomes:
		a5
		a4
		a1
		a2
		a3
	*/

	//debug_print("Entered an Expression!",0,0);
	if(	ee->kind == EXPR_FCALL ||
		ee->kind == EXPR_METHOD ||
		ee->kind == EXPR_BUILTIN_CALL ||
		ee->kind == EXPR_CALLFNPTR
	){
		uint64_t loc = ast_vm_stack_push_temporary(NULL, ee->t);
		vm_stack[loc].t = ee->t;
		saved_vstack_pointer = vm_stackpointer; //NOTE: saved after pushing the temporary. Before subexpressions.
		for(i = MAX_FARGS-1; i >= 0; i--)
		{
			if(ee->subnodes[i])
			{
				do_expr(ee->subnodes[i]); 
				n_subexpressions++;
			}
			if(i == 0) break;
		}
		//IMPORTANT NOTE: for expr_callfnptr, the function pointer itself is subnodes[0],
		//so it ends up on top!
	}else{ /*Otherwise, in-order.*/
		saved_vstack_pointer = vm_stackpointer; //saved before calling sub expressions.
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

	if(ee->kind == EXPR_GETFNPTR){
		uint64_t general;
		//push a temporary.
		general = ast_vm_stack_push_temporary(NULL, ee->t);
		vm_stack[general].smalldata = (uint64_t) (symbol_table + ee->symid);
		return;
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
		saved_cur_func_frame_size = cur_func_frame_size;
		saved_cur_expr_stack_usage = cur_expr_stack_usage;
		saved_active_function= active_function;
		saved_vstack_pointer = vm_stackpointer;
		ast_execute_function(symbol_table + ee->symid);
		
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
	if(ee->kind == EXPR_CALLFNPTR){
		uint64_t retrieved_pointer;
		int64_t i;
		//int found = 0;
		retrieved_pointer = vm_stack[vm_stackpointer-1].smalldata; //function pointer was on top.
		ast_vm_stack_pop();
		saved_cur_func_frame_size = cur_func_frame_size;
		saved_cur_expr_stack_usage = cur_expr_stack_usage;
		saved_active_function= active_function;
		saved_vstack_pointer = vm_stackpointer;
		
		ast_execute_function((symdecl*)retrieved_pointer);
		cur_func_frame_size = saved_cur_func_frame_size;
		cur_expr_stack_usage = saved_cur_expr_stack_usage;
		active_function = saved_active_function;
		vm_stackpointer = saved_vstack_pointer;
		for(i = 0; i < (int64_t)ee->fnptr_nargs; i++) {ast_vm_stack_pop();}
		return;
	}
	if(ee->kind == EXPR_LSYM || ee->kind == EXPR_GSYM || ee->kind == EXPR_GETGLOBALPTR){
		void* p;		
		uint64_t general;
		if(ee->kind == EXPR_LSYM)
			p = retrieve_variable_memory_pointer(ee->symname, 0);
		if(ee->kind == EXPR_GSYM || ee->kind == EXPR_GETGLOBALPTR)
			p = retrieve_variable_memory_pointer(ee->symname, 1);
/*
		debug_print("Retrieved symbol variable memory pointer.",0,0);
		debug_print("Its name is:",0,0);
		debug_print(ee->symname,0,0);
		debug_print("Its address is:",(uint64_t)p,0);
*/
		general = ast_vm_stack_push_temporary(NULL, ee->t);
		memcpy(
			&vm_stack[general].smalldata,
			&p,
			POINTER_SIZE
		);
		return;
	}
	if(ee->kind == EXPR_STRINGLIT){
		void* p;
		uint64_t general;
		p = symbol_table[ee->symid].cdata;
		//debug_print("Found String Literal. Address:",(uint64_t)p,0);
		general = ast_vm_stack_push_temporary(NULL, ee->t);
		memcpy(&vm_stack[general].smalldata,&p,POINTER_SIZE);
		return;
	}
	if(ee->kind == EXPR_MEMBER){ /*the actual operation is to get a pointer to the member.*/
		void* p;
		uint64_t pt;
		uint64_t levels_of_indirection;
		pt = vm_stack[vm_stackpointer-1].smalldata;
		levels_of_indirection = ee->subnodes[0]->t.pointerlevel;
		//if(ee->subnodes[0]->t.is_lvalue != 0) levels_of_indirection++;
		/*
			You can access the members of a struct with arbitrary levels of indirection.
			and if it is more than one, then we need to dereference a pointer-to-pointer.
		*/
	//	debug_print("Pointer to struct was (before):", (uint64_t)pt,0);
		while(levels_of_indirection > 1){
			memcpy(&p, &pt, POINTER_SIZE);
			memcpy(
				&pt,
				p,
				POINTER_SIZE
			);
			levels_of_indirection--;
			//debug_print("Pointer to struct was (iter):", (uint64_t)pt,0);
		}
	//	debug_print("Pointer to struct was (after):", (uint64_t)pt,0);
		/*
			pt holds single-level pointer-to-struct.

			Now, we need to add the offset of the member...
		*/
		{
			uint64_t off_of;
			/*off_of = get_offsetof(
							type_table + ee->subnodes[0]->t.structid, 
							ee->symname
						);*/
			off_of = ee->idata;
			//debug_print("Accessing member, off_of was...", off_of,0);
			pt = pt + off_of;
		}
//		debug_print("<EXPR_MEMBER> here's the final address:", (uint64_t)pt,0);
	//	if(((int*)pt)[0] == 3){
	//		debug_print("<Overhead> I can see it... It's here:", pt,0);
	//	}
		vm_stack[vm_stackpointer-1].smalldata = pt;
		vm_stack[vm_stackpointer-1].t = ee->t;
		return;
	}

	if(ee->kind == EXPR_MEMBERPTR){ /*the actual operation is to get a pointer to the member.*/
		void* p;
		uint64_t pt;
		uint64_t levels_of_indirection;
		pt = vm_stack[vm_stackpointer-1].smalldata;
		levels_of_indirection = ee->subnodes[0]->t.pointerlevel;
		//if(ee->subnodes[0]->t.is_lvalue != 0) levels_of_indirection++;
		/*
			You can access the members of a struct with arbitrary levels of indirection.
			and if it is more than one, then we need to dereference a pointer-to-pointer.
		*/
	//	debug_print("Pointer to struct was (before):", (uint64_t)pt,0);
		while(levels_of_indirection > 1){
			memcpy(&p, &pt, POINTER_SIZE);
			memcpy(
				&pt,
				p,
				POINTER_SIZE
			);
			levels_of_indirection--;
			//debug_print("Pointer to struct was (iter):", (uint64_t)pt,0);
		}
	//	debug_print("Pointer to struct was (after):", (uint64_t)pt,0);
		/*
			pt holds single-level pointer-to-struct.

			Now, we need to add the offset of the member...
		*/
		{
			uint64_t off_of;
			/*off_of = get_offsetof(
							type_table + ee->subnodes[0]->t.structid, 
							ee->symname
						);*/
			off_of = ee->idata;
			//debug_print("Accessing member, off_of was...", off_of,0);
			pt = pt + off_of;
		}
//		debug_print("<EXPR_MEMBER> here's the final address:", (uint64_t)pt,0);
	//	if(((int*)pt)[0] == 3){
	//		debug_print("<Overhead> I can see it... It's here:", pt,0);
	//	}
		vm_stack[vm_stackpointer-1].smalldata = pt;
		vm_stack[vm_stackpointer-1].t = ee->t;
		return;
	}
	if(ee->kind == EXPR_MOVE){
		//POINTER_SIZE
		void* p1;
		void* p2;
		type t;
		memcpy(&p1, &vm_stack[vm_stackpointer-2].smalldata, POINTER_SIZE);
		memcpy(&p2, &vm_stack[vm_stackpointer-1].smalldata, POINTER_SIZE);
		t = ee->t;
		t.pointerlevel--; //it's actually one less.
		memcpy(p1, p2, type_getsz(t));
		ast_vm_stack_pop(); //no longer need the second operand.
		//
		vm_stack[vm_stackpointer-1].smalldata = (uint64_t)p2;
		vm_stack[vm_stackpointer-1].t = ee->t;
		return;
	}
	if(ee->kind == EXPR_ASSIGN){
		void* p1;
		uint64_t val;
		//it's an lvalue... therefore, a pointer
		memcpy(
			&p1, 
			&vm_stack[vm_stackpointer-2].smalldata, 
			POINTER_SIZE
		);
		//grab some number of bytes...
		//debug_print("@Assignment! Pointer is:",(uint64_t)p1,0);

		//If it's a pointer, we copy 8 bytes.
		if(ee->subnodes[0]->t.pointerlevel > 0){
			memcpy(
				&val, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				POINTER_SIZE
			);
			memcpy(p1, &val, POINTER_SIZE);
			//debug_print("<DEBUG ASSIGNMENT> Val was:",val,0);
			goto end_expr_assign;
		}
		if(
			ee->subnodes[0]->t.basetype == BASE_I64 || 
			ee->subnodes[0]->t.basetype == BASE_F64 ||
			ee->subnodes[0]->t.basetype == BASE_U64
		)
		{
			memcpy(
				&val, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				8
			);
			memcpy(p1, &val, 8);
			goto end_expr_assign;
		}
		if(
			ee->subnodes[0]->t.basetype == BASE_I32 || 
			ee->subnodes[0]->t.basetype == BASE_F32 ||
			ee->subnodes[0]->t.basetype == BASE_U32
		)
		{
			memcpy(
				&val, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				4
			);
			memcpy(p1, &val, 4);
			goto end_expr_assign;
		}
		if(
			ee->subnodes[0]->t.basetype == BASE_I16 ||
			ee->subnodes[0]->t.basetype == BASE_U16
		)
		{
			memcpy(
				&val, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				2
			);
			memcpy(p1, &val, 2);
			goto end_expr_assign;
		}
		if(
			ee->subnodes[0]->t.basetype == BASE_I8 ||
			ee->subnodes[0]->t.basetype == BASE_U8
		)
		{
			memcpy(
				&val, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				1
			);
			memcpy(p1, &val, 1);
			goto end_expr_assign;
		}
		puts("VM internal error");
		puts("Unhandled assignment type.");
		exit(1);

		end_expr_assign:;
		ast_vm_stack_pop(); //no longer need the second operand.
		//assignment now yields a void...
		vm_stack[vm_stackpointer-1].smalldata = 0;
		vm_stack[vm_stackpointer-1].t = type_init();
		return;
	}

	if(
		ee->kind == EXPR_PRE_INCR ||
		ee->kind == EXPR_POST_INCR ||
		ee->kind == EXPR_PRE_DECR ||
		ee->kind == EXPR_POST_DECR
	){
		void* p1;
		uint64_t val;
		uint64_t valpre;
		int32_t i32data;
		int16_t i16data;
		int8_t i8data;
		float f32data;
		double f64data;
		int is_incr;
		int is_pre;
		is_incr= 
		(
			ee->kind == EXPR_PRE_INCR || 
			ee->kind == EXPR_POST_INCR
		);
		is_pre = 
		(
			ee->kind == EXPR_PRE_INCR || 
			ee->kind == EXPR_PRE_DECR
		);
		//get lvalue's address.
		memcpy(
			&p1, 
			&vm_stack[vm_stackpointer-1].smalldata, 
			POINTER_SIZE
		);
		//incrementing a pointer.
		if(ee->t.pointerlevel > 0){
			uint64_t how_much_to_change;
			//grab the actual value at that address...
			memcpy(&val, p1, 8); valpre = val;
			//type we are pointing to...
			type t2;
			t2 = ee->t;
			t2.pointerlevel--;
			//we shift by that size
			how_much_to_change = type_getsz(t2);
			//shift
			if(is_incr)
				val = val + how_much_to_change;
			if(!is_incr)
				val = val - how_much_to_change;
			memcpy(p1, &val, POINTER_SIZE);
			goto end_expr_pre_incr;
		}
		if(
			ee->t.basetype == BASE_I64 || 
			ee->t.basetype == BASE_U64
		)
		{
			memcpy(&val, p1, 8); valpre = val;
			//debug_print("Valpre:", valpre,0);
			if(is_incr) val++;
			if(!is_incr) val--;
			memcpy(p1, &val, 8);
			goto end_expr_pre_incr;
		}
		if(
			ee->t.basetype == BASE_I32 || 
			ee->t.basetype == BASE_U32
		)
		{
			memcpy(&val, p1, 4); valpre = val;
			memcpy(&i32data, &val, 4);
			if(is_incr) i32data++;
			if(!is_incr) i32data--;
			memcpy(p1, &i32data, 4);
			goto end_expr_pre_incr;
		}
		if(
			ee->t.basetype == BASE_I16 ||
			ee->t.basetype == BASE_U16
		)
		{
			memcpy(&val, p1, 2); valpre = val;
			memcpy(&i16data, &val, 2);
			if(is_incr) i16data++;
			if(!is_incr) i16data--;
			memcpy(p1, &i16data, 2);
			goto end_expr_pre_incr;
		}
		if(
			ee->t.basetype == BASE_I8 ||
			ee->t.basetype == BASE_U8
		)
		{
			memcpy(&val, p1, 1); valpre = val;
			memcpy(&i8data, &val, 1);
			if(is_incr) i8data++;
			if(!is_incr) i8data--;
			memcpy(p1, &i8data, 1);
			goto end_expr_pre_incr;
		}
		if(ee->t.basetype == BASE_F64){
			memcpy(&val, p1, 8); valpre = val;
			memcpy(&f64data, &val, 8);
			if(is_incr)f64data = f64data + 1;
			if(!is_incr)f64data = f64data - 1;
			memcpy(p1, &f64data, 8);
			goto end_expr_pre_incr;
		}	
		if(ee->t.basetype == BASE_F32){
			memcpy(&val, p1, 4); valpre = val;
			memcpy(&f32data, &val, 4);
			if(is_incr)f32data = f32data + 1;
			if(!is_incr)f32data = f32data - 1;
			memcpy(p1, &f32data, 4);
			goto end_expr_pre_incr;
		}
		
		puts("VM internal error");
		puts("Unhandled pre/post incr/decr type.");
		if(is_pre) puts("was pre");
		if(!is_pre) puts("was post");
		if(is_incr) puts("was incr");
		if(!is_incr) puts("was decr");
		exit(1);

		end_expr_pre_incr:;
		if(is_pre)
			vm_stack[vm_stackpointer-1].smalldata = val;
		if(!is_pre)
			vm_stack[vm_stackpointer-1].smalldata = valpre;
		vm_stack[vm_stackpointer-1].t = ee->t;
		return;
	}


	
	if(ee->kind == EXPR_CAST){
		uint64_t data;
		data = vm_stack[vm_stackpointer-1].smalldata;
		/*
		if is_lvalue, then small data was a pointer!
		do a dereference...
		*/
//		debug_print("Doing a cast...",0,0);
		if(ee->subnodes[0]->t.is_lvalue){
			//debug_print("Undoing LVALUE, currently it is:",data,0);
			memcpy(
				&data, 
				(void*)data,
				type_getsz(ee->subnodes[0]->t)
			);
	//		debug_print("Undid LVALUE, the result as a u64 was:",data,0);
		}
		/*If they were both pointers, we do nothing.*/
		if(ee->t.pointerlevel > 0)
			if(ee->subnodes[0]->t.pointerlevel > 0)
				goto end_expr_cast;
	/*
		if(ee->t.pointerlevel > 0)
			if(ee->subnodes[0]->t.basetype == BASE_U64 ||
				ee->subnodes[0]->t.basetype == BASE_I64
			){
				goto end_expr_cast;
			}
		if(	
			ee->t.basetype == BASE_U64 ||
			ee->t.basetype == BASE_I64
		)
			if(ee->subnodes[0]->t.pointerlevel > 0)
				goto end_expr_cast;
	*/
		
		/*TO POINTER.*/
		if(ee->t.pointerlevel){
			data = do_primitive_type_conversion(
				data,
				vm_stack[vm_stackpointer-1].t.basetype, //srcbase
				BASE_U64 //whatbase
			);
			vm_stack[vm_stackpointer-1].smalldata = data;
			goto end_expr_cast;
		}
		/*FROM POINTER*/
		if(ee->subnodes[0]->t.pointerlevel > 0){
			data = do_primitive_type_conversion(
				data,
				BASE_U64, //srcbase
				ee->t.basetype //whatbase
			);
			vm_stack[vm_stackpointer-1].smalldata = data;
			goto end_expr_cast;
		}
		/*CONVERSION WITHOUT POINTERS*/
		data = do_primitive_type_conversion(
			data,
			vm_stack[vm_stackpointer-1].t.basetype,
			ee->t.basetype
		);
		end_expr_cast:;
		//debug_print("Cast ends with:",data,0);
		vm_stack[vm_stackpointer-1].smalldata = data;
		//debug_print("The value is...", vm_stack[vm_stackpointer-1].smalldata,0);
		vm_stack[vm_stackpointer-1].t = ee->t;
		return;
	}
	if(ee->kind == EXPR_INDEX){
		uint64_t data;
		int64_t ind;
		void* p;
		/*the i64 is on top...*/
		ind = vm_stack[vm_stackpointer-1].smalldata;
		/*the pointer is on bottom.*/
		data = vm_stack[vm_stackpointer-2].smalldata;
		/*move it to pointer memory!*/
		memcpy(&p, &data, POINTER_SIZE);
		
		//perform the pointer addition.
		p = do_ptradd(p, ind, type_getsz(ee->t));
		memcpy(
			&vm_stack[vm_stackpointer-2].smalldata, 
			&p, 
			POINTER_SIZE
		);
		ast_vm_stack_pop(); //toss the second operand.
		vm_stack[vm_stackpointer-1].t = ee->t;
		return;
	}
	if(
		ee->kind == EXPR_ADD ||
		ee->kind == EXPR_SUB ||
		ee->kind == EXPR_MUL
	){
		int32_t i32data1;	
		int32_t i32data2;	
		int16_t i16data1;
		int16_t i16data2;
		int8_t i8data1;
		int8_t i8data2;
		float f32data1;
		float f32data2;
		double f64data1;
		double f64data2;
		int optype;
		optype = ee->kind;
		/*case 0: pointer +- int*/
		if(ee->subnodes[0]->t.pointerlevel > 0){
			uint64_t a;
			uint64_t b;
			type t;
			a = vm_stack[vm_stackpointer-2].smalldata;
			b = vm_stack[vm_stackpointer-1].smalldata;
			t = ee->subnodes[0]->t; //the pointer is the first one.
			t.pointerlevel--;
			b *= type_getsz(t);
			if(optype == EXPR_ADD) a = a + b;
			if(optype == EXPR_SUB) a = a - b;
			vm_stack[vm_stackpointer-2].smalldata = a;
			goto end_expr_add;
		}
		/*case 0.5: int + pointer*/
		if(ee->subnodes[1]->t.pointerlevel > 0){
			uint64_t a;
			uint64_t b;
			type t;
			a = vm_stack[vm_stackpointer-2].smalldata;
			b = vm_stack[vm_stackpointer-1].smalldata;
			t = ee->subnodes[1]->t; //the pointer is the second one.
			t.pointerlevel--;
			a *= type_getsz(t);
			if(optype == EXPR_ADD) a = a + b;
			vm_stack[vm_stackpointer-2].smalldata = a;
			goto end_expr_add;
		}
		
		/*case 1: both are i64 or u64, or pointer arithmetic.*/
		if(
			ee->subnodes[0]->t.basetype == BASE_U64 ||
			ee->subnodes[0]->t.basetype == BASE_I64
		){
			/*POINTER_SIZE*/
			if(optype == EXPR_ADD)
				vm_stack[vm_stackpointer-2].smalldata = 
					vm_stack[vm_stackpointer-2].smalldata + 
					vm_stack[vm_stackpointer-1].smalldata
				;	
			if(optype == EXPR_SUB)
				vm_stack[vm_stackpointer-2].smalldata = 
					vm_stack[vm_stackpointer-2].smalldata -
					vm_stack[vm_stackpointer-1].smalldata
				;
			if(optype == EXPR_MUL)
				vm_stack[vm_stackpointer-2].smalldata = 
					vm_stack[vm_stackpointer-2].smalldata *
					vm_stack[vm_stackpointer-1].smalldata
				;
			goto end_expr_add;
		}
		/*case 3: both are i32 or u32*/
		if(
			ee->subnodes[0]->t.basetype == BASE_U32 ||
			ee->subnodes[0]->t.basetype == BASE_I32
		){
			memcpy(
				&i32data1, 
				&vm_stack[vm_stackpointer-2].smalldata, 
				4
			);
			memcpy(
				&i32data2, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				4
			);
			if(optype == EXPR_ADD) i32data1 = i32data1 + i32data2;
			if(optype == EXPR_SUB) i32data1 = i32data1 - i32data2;
			if(optype == EXPR_MUL) i32data1 = i32data1 * i32data2;
			memcpy(
				&vm_stack[vm_stackpointer-2].smalldata, 
				&i32data1, 
				4
			);
			goto end_expr_add;
		}
		/*case 4: both are i16 or u16*/
		if(
			ee->subnodes[0]->t.basetype == BASE_U16 ||
			ee->subnodes[0]->t.basetype == BASE_I16
		){
			memcpy(
				&i16data1, 
				&vm_stack[vm_stackpointer-2].smalldata, 
				2
			);
			memcpy(
				&i16data2, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				2
			);
			if(optype == EXPR_ADD) i16data1 = i16data1 + i16data2;
			if(optype == EXPR_SUB) i16data1 = i16data1 - i16data2;
			if(optype == EXPR_MUL) i16data1 = i16data1 * i16data2;
			memcpy(
				&vm_stack[vm_stackpointer-2].smalldata, 
				&i16data1, 
				2
			);
			goto end_expr_add;
		}	
		/*case 5: both are i8 or u8*/
		if(
			ee->subnodes[0]->t.basetype == BASE_U8 ||
			ee->subnodes[0]->t.basetype == BASE_I8
		){
			memcpy(
				&i8data1, 
				&vm_stack[vm_stackpointer-2].smalldata, 
				1
			);
			memcpy(
				&i8data2, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				1
			);
			if(optype == EXPR_ADD) i8data1 = i8data1 + i8data2;
			if(optype == EXPR_SUB) i8data1 = i8data1 - i8data2;
			if(optype == EXPR_MUL) i8data1 = i8data1 * i8data2;
			memcpy(
				&vm_stack[vm_stackpointer-2].smalldata, 
				&i8data1, 
				1
			);
			goto end_expr_add;
		}
		/*Case 6: both are f32*/
		if(
			ee->subnodes[0]->t.basetype == BASE_F32
		){
			memcpy(
				&f32data1, 
				&vm_stack[vm_stackpointer-2].smalldata, 
				4
			);
			memcpy(
				&f32data2, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				4
			);
			if(optype == EXPR_ADD)f32data1 = f32data1 + f32data2;
			if(optype == EXPR_SUB)f32data1 = f32data1 - f32data2;
			if(optype == EXPR_MUL)f32data1 = f32data1 * f32data2;
			memcpy(
				&vm_stack[vm_stackpointer-2].smalldata, 
				&f32data1, 
				4
			);
			goto end_expr_add;
		}		/*Case 6: both are f64*/
		if(
			ee->subnodes[0]->t.basetype == BASE_F64
		){
			memcpy(
				&f64data1, 
				&vm_stack[vm_stackpointer-2].smalldata, 
				8
			);
			memcpy(
				&f64data2, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				8
			);
			if(optype == EXPR_ADD)f64data1 = f64data1 + f64data2;
			if(optype == EXPR_SUB)f64data1 = f64data1 - f64data2;
			if(optype == EXPR_MUL)f64data1 = f64data1 * f64data2;
			memcpy(
				&vm_stack[vm_stackpointer-2].smalldata, 
				&f64data1, 
				8
			);
			goto end_expr_add;
		}
		puts("VM ERROR");
		puts("Unhandled add/sub/mul type.");
		exit(1);

		end_expr_add:;
		ast_vm_stack_pop();  //we no longer need the second operand.
		vm_stack[vm_stackpointer-1].t = ee->t;
		return;
	}
	if(
		ee->kind == EXPR_LSH ||
		ee->kind == EXPR_RSH ||
		ee->kind == EXPR_BITOR ||
		ee->kind == EXPR_BITXOR ||
		ee->kind == EXPR_BITAND ||
		ee->kind == EXPR_LOGAND ||
		ee->kind == EXPR_LOGOR
	){
		int64_t a;
		int64_t b;
		int op = ee->kind;
		a = vm_stack[vm_stackpointer-2].smalldata;
		b = vm_stack[vm_stackpointer-1].smalldata;
		if(op == EXPR_LSH) a = a<<b;
		if(op == EXPR_RSH) a = a>>b;
		if(op == EXPR_BITOR) a = a|b;
		if(op == EXPR_BITXOR) a = a^b;
		if(op == EXPR_BITAND) a = a&b;
		if(op == EXPR_LOGAND) a = a&&b;
		if(op == EXPR_LOGOR) a = a||b;
		vm_stack[vm_stackpointer-2].smalldata = a;
		vm_stack[vm_stackpointer-2].t = ee->t;
		ast_vm_stack_pop(); //we no longer need the second operand.
		return;
	}

	if(
		ee->kind == EXPR_COMPL ||
		ee->kind == EXPR_NOT
	){
		int64_t b;
		int op = ee->kind;
		b = vm_stack[vm_stackpointer-1].smalldata;
		if(op == EXPR_COMPL) b = ~b;
		if(op == EXPR_NOT) b = !b;
		vm_stack[vm_stackpointer-1].smalldata = b;
		vm_stack[vm_stackpointer-1].t = ee->t;
		return;
	}
	if(ee->kind == EXPR_NEG){
		int64_t i64data;
		int32_t i32data;
		int16_t i16data;
		int8_t i8data;
		float f32_1;
		double f64_1;

		
		/*Case I64*/
		if(
			ee->t.basetype == BASE_I64
		){
			memcpy(
				&i64data, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				8
			);
			i64data = -1 * i64data;
			memcpy(
				&vm_stack[vm_stackpointer-1].smalldata, 
				&i64data, 
				8
			);
			goto end_expr_neg;
		}		
		/*Case I32*/
		if(
			ee->t.basetype == BASE_I32
		){
			memcpy(
				&i32data, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				4
			);
			i32data = -1 * i32data;
			memcpy(
				&vm_stack[vm_stackpointer-1].smalldata, 
				&i32data, 
				4
			);
			goto end_expr_neg;
		}		
		/*Case I16*/
		if(
			ee->t.basetype == BASE_I16
		){
			memcpy(
				&i16data, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				2
			);
			i16data = -1 * i16data;
			memcpy(
				&vm_stack[vm_stackpointer-1].smalldata, 
				&i16data, 
				2
			);
			goto end_expr_neg;
		}		
		/*Case I8*/
		if(
			ee->t.basetype == BASE_I8
		){
			memcpy(
				&i8data, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				1
			);
			i8data = -1 * i8data;
			memcpy(
				&vm_stack[vm_stackpointer-1].smalldata, 
				&i8data, 
				1
			);
			goto end_expr_neg;
		}
		/*Case: f32*/
		if(
			ee->t.basetype == BASE_F32
		){
			memcpy(
				&f32_1, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				4
			);
			f32_1 = -1 * f32_1;
			memcpy(
				&vm_stack[vm_stackpointer-1].smalldata, 
				&f32_1, 
				4
			);
			goto end_expr_neg;
		}

		/*Case: f64*/
		if(
			ee->t.basetype == BASE_F64
		){
			memcpy(
				&f64_1, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				8
			);
			f64_1 = -1 * f64_1;
			memcpy(
				&vm_stack[vm_stackpointer-1].smalldata, 
				&f64_1, 
				8
			);
			goto end_expr_neg;
		}
		puts("VM ERROR");
		puts("Unhandled neg type.");
		exit(1);
		end_expr_neg:;
		vm_stack[vm_stackpointer-1].t = ee->t;
		return;
	}
	if(
		ee->kind == EXPR_DIV ||
		ee->kind == EXPR_MOD
	)
	{
		int64_t i64data1;
		int64_t i64data2;		
		uint64_t u64data1;
		uint64_t u64data2;
		int32_t i32data1;
		int32_t i32data2;
		uint32_t u32data1;
		uint32_t u32data2;
		int16_t i16data1;
		int16_t i16data2;
		uint16_t u16data1;		
		uint16_t u16data2;	
		int8_t i8data1;
		int8_t i8data2;
		uint8_t u8data1;
		uint8_t u8data2;
		float f32_1;
		float f32_2;
		double f64_1;
		double f64_2;
		int op = ee->kind;
		/*case 1: both are u64*/
		if(
			ee->subnodes[0]->t.basetype == BASE_U64
		){
			unsigned SIZE_TO_COPY;
			SIZE_TO_COPY = 
			8;
			u64data1 = vm_stack[vm_stackpointer-2].smalldata;
			u64data2 = vm_stack[vm_stackpointer-1].smalldata;
			if(op == EXPR_DIV) u64data1 = u64data1 / u64data2;
			if(op == EXPR_MOD) u64data1 = u64data1 % u64data2;
			memcpy(
				&vm_stack[vm_stackpointer-2].smalldata, 
				&u64data1, 
				SIZE_TO_COPY
			);
			goto end_expr_div;
		}		
		if(
			ee->subnodes[0]->t.basetype == BASE_I64
		){
			unsigned SIZE_TO_COPY;
			SIZE_TO_COPY = 
			8;
			i64data1 = vm_stack[vm_stackpointer-2].smalldata;
			i64data2 = vm_stack[vm_stackpointer-1].smalldata;
			if(op == EXPR_DIV) i64data1 = i64data1 / i64data2;
			if(op == EXPR_MOD) i64data1 = i64data1 % i64data2;
			memcpy(
				&vm_stack[vm_stackpointer-2].smalldata, 
				&i64data1, 
				SIZE_TO_COPY
			);
			goto end_expr_div;
		}
		/*32 bit int*/
		if(
			ee->subnodes[0]->t.basetype == BASE_U32
		){
			unsigned SIZE_TO_COPY;
			SIZE_TO_COPY = 
			4;
			memcpy(
				&u32data1, 
				&vm_stack[vm_stackpointer-2].smalldata, 
				SIZE_TO_COPY
			);
			memcpy(
				&u32data2, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				SIZE_TO_COPY
			);
			if(op == EXPR_DIV) u32data1 = u32data1 / u32data2;
			if(op == EXPR_MOD) u32data1 = u32data1 % u32data2;
			memcpy(
				&vm_stack[vm_stackpointer-2].smalldata, 
				&u32data1, 
				SIZE_TO_COPY
			);
			goto end_expr_div;
		}
		if(
			ee->subnodes[0]->t.basetype == BASE_I32
		){
			unsigned SIZE_TO_COPY;
			SIZE_TO_COPY = 
			4;
			memcpy(
				&i32data1, 
				&vm_stack[vm_stackpointer-2].smalldata, 
				SIZE_TO_COPY
			);
			memcpy(
				&i32data2, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				SIZE_TO_COPY
			);
			if(op == EXPR_DIV) i32data1 = i32data1 / i32data2;
			if(op == EXPR_MOD) i32data1 = i32data1 % i32data2;
			memcpy(
				&vm_stack[vm_stackpointer-2].smalldata, 
				&i32data1, 
				SIZE_TO_COPY
			);
			goto end_expr_div;
		}
		/*16 bit int*/
		if(
			ee->subnodes[0]->t.basetype == BASE_U16
		){
			unsigned SIZE_TO_COPY;
			SIZE_TO_COPY = 
			2;
			memcpy(
				&u16data1, 
				&vm_stack[vm_stackpointer-2].smalldata, 
				SIZE_TO_COPY
			);
			memcpy(
				&u16data2, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				SIZE_TO_COPY
			);
			if(op == EXPR_DIV) u16data1 = u16data1 / u16data2;
			if(op == EXPR_MOD) u16data1 = u16data1 % u16data2;
			memcpy(
				&vm_stack[vm_stackpointer-2].smalldata, 
				&u16data1, 
				SIZE_TO_COPY
			);
			goto end_expr_div;
		}
		if(
			ee->subnodes[0]->t.basetype == BASE_I16
		){
			unsigned SIZE_TO_COPY;
			SIZE_TO_COPY = 
			2;
			memcpy(
				&i16data1, 
				&vm_stack[vm_stackpointer-2].smalldata, 
				SIZE_TO_COPY
			);
			memcpy(
				&i16data2, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				SIZE_TO_COPY
			);
			if(op == EXPR_DIV) i16data1 = i16data1 / i16data2;
			if(op == EXPR_MOD) i16data1 = i16data1 % i16data2;
			memcpy(
				&vm_stack[vm_stackpointer-2].smalldata, 
				&i16data1, 
				SIZE_TO_COPY
			);
			goto end_expr_div;
		}
		/*8 bit int*/
		if(
			ee->subnodes[0]->t.basetype == BASE_U8
		){
			unsigned SIZE_TO_COPY;
			SIZE_TO_COPY = 
			1;
			memcpy(
				&u8data1, 
				&vm_stack[vm_stackpointer-2].smalldata, 
				SIZE_TO_COPY
			);
			memcpy(
				&u8data2, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				SIZE_TO_COPY
			);
			if(op == EXPR_DIV) u8data1 = u8data1 / u8data2;
			if(op == EXPR_MOD) u8data1 = u8data1 % u8data2;
			memcpy(
				&vm_stack[vm_stackpointer-2].smalldata, 
				&u8data1, 
				SIZE_TO_COPY
			);
			goto end_expr_div;
		}
		if(
			ee->subnodes[0]->t.basetype == BASE_I8
		){
			unsigned SIZE_TO_COPY;
			SIZE_TO_COPY = 
			1;
			memcpy(
				&i8data1, 
				&vm_stack[vm_stackpointer-2].smalldata, 
				SIZE_TO_COPY
			);
			memcpy(
				&i8data2, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				SIZE_TO_COPY
			);
			if(op == EXPR_DIV) i8data1 = i8data1 / i8data2;
			if(op == EXPR_MOD) i8data1 = i8data1 % i8data2;
			memcpy(
				&vm_stack[vm_stackpointer-2].smalldata, 
				&i8data1, 
				SIZE_TO_COPY
			);
			goto end_expr_div;
		}
		/*Case 6: both are f32*/
		if(
			ee->subnodes[0]->t.basetype == BASE_F32
		){
			unsigned SIZE_TO_COPY;
			SIZE_TO_COPY = 
			4;
			memcpy(
				&f32_1, 
				&vm_stack[vm_stackpointer-2].smalldata, 
				SIZE_TO_COPY
			);
			memcpy(
				&f32_2, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				SIZE_TO_COPY
			);
			f32_1 = f32_1 / f32_2;
			memcpy(
				&vm_stack[vm_stackpointer-2].smalldata, 
				&f32_1, 
				SIZE_TO_COPY
			);
			goto end_expr_div;
		}
		/*Case 7: both are f64*/
		if(
			ee->subnodes[0]->t.basetype == BASE_F64
		){
			unsigned SIZE_TO_COPY;
			SIZE_TO_COPY = 
			8;
			memcpy(
				&f64_1, 
				&vm_stack[vm_stackpointer-2].smalldata, 
				SIZE_TO_COPY
			);
			memcpy(
				&f64_2, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				SIZE_TO_COPY
			);
			//we don't have fmod
			f64_1 = f64_1 / f64_2;
			memcpy(
				&vm_stack[vm_stackpointer-2].smalldata, 
				&f64_1, 
				SIZE_TO_COPY
			);
			goto end_expr_div;
		}
		puts("VM ERROR");
		puts("Unhandled div/mod type.");
		exit(1);
		end_expr_div:;
		ast_vm_stack_pop(); //get rid of the second operand.
		vm_stack[vm_stackpointer-1].t = ee->t;
		return;
	}
	/*
		COMPARISONS
	*/

	/*Make things easier for ourselves... We are going to eat them, anyway.*/
	if(ee->kind == EXPR_EQ || ee->kind == EXPR_NEQ){
		if(ee->subnodes[0]->t.pointerlevel > 0){
			vm_stack[vm_stackpointer-1].t.basetype = BASE_U64;
			vm_stack[vm_stackpointer-1].t.pointerlevel = 0;
			vm_stack[vm_stackpointer-2].t.basetype = BASE_U64;
			vm_stack[vm_stackpointer-2].t.pointerlevel = 0;
		}
	}
	if(ee->kind == EXPR_STREQ){
		char* p1;
		char* p2;
		p1 = (char*)vm_stack[vm_stackpointer-2].smalldata;
		p2 = (char*)vm_stack[vm_stackpointer-1].smalldata;
		if(p1 == NULL || p2 == NULL){
			puts("VM error");
			puts("streq got NULL");
			puts("In function:");
			puts(symbol_table[active_function].name);
			exit(1);
		}
		ast_vm_stack_pop(); //we no longer need the index itself.
		vm_stack[vm_stackpointer-1].t = ee->t;
		vm_stack[vm_stackpointer-1].smalldata = !!streq(p1,p2);
		return;
	}
	if(ee->kind == EXPR_STRNEQ){
		char* p1;
		char* p2;
		p1 = (char*)vm_stack[vm_stackpointer-2].smalldata;
		p2 = (char*)vm_stack[vm_stackpointer-1].smalldata;
		if(p1 == NULL || p2 == NULL){
			puts("VM error");
			puts("streq got NULL");
			puts("In function:");
			puts(symbol_table[active_function].name);
			exit(1);
		}
		ast_vm_stack_pop(); //we no longer need the index itself.
		vm_stack[vm_stackpointer-1].t = ee->t;
		vm_stack[vm_stackpointer-1].smalldata = !streq(p1,p2);
		return;
	}
	if(
		ee->kind == EXPR_LT ||
		ee->kind == EXPR_LTE ||
		ee->kind == EXPR_GTE ||
		ee->kind == EXPR_GT ||
		ee->kind == EXPR_EQ ||
		ee->kind == EXPR_NEQ
	)
	{
		int64_t i64data1;
		int64_t i64data2;		
		uint64_t u64data1;
		uint64_t u64data2;
		int32_t i32data1;
		int32_t i32data2;
		uint32_t u32data1;
		uint32_t u32data2;
		int16_t i16data1;
		int16_t i16data2;
		uint16_t u16data1;		
		uint16_t u16data2;	
		int8_t i8data1;
		int8_t i8data2;
		uint8_t u8data1;
		uint8_t u8data2;
		float f32data1;
		float f32data2;
		double f64data1;
		double f64data2;
		int op = ee->kind;
		/*case 1: both are u64*/
		if(
			vm_stack[vm_stackpointer-1].t.basetype == BASE_U64
		){
			u64data1 = vm_stack[vm_stackpointer-2].smalldata;
			u64data2 = vm_stack[vm_stackpointer-1].smalldata;
			if(op == EXPR_LT)  i64data1 = u64data1 < u64data2;
			if(op == EXPR_LTE) i64data1 = u64data1 <= u64data2;
			if(op == EXPR_GTE) i64data1 = u64data1 >= u64data2;
			if(op == EXPR_GT)  i64data1 = u64data1 > u64data2;
			if(op == EXPR_EQ)  i64data1 = u64data1 == u64data2;
			if(op == EXPR_NEQ) i64data1 = u64data1 != u64data2;
			memcpy(&vm_stack[vm_stackpointer-2].smalldata, &i64data1, 8);
			goto end_expr_lt;
		}		

		/*case 2: both are i64*/
		if(
			vm_stack[vm_stackpointer-1].t.basetype == BASE_I64
		){
			i64data1 = vm_stack[vm_stackpointer-2].smalldata;
			i64data2 = vm_stack[vm_stackpointer-1].smalldata;
			if(op == EXPR_LT)  i64data1 = i64data1 < i64data2;
			if(op == EXPR_LTE) i64data1 = i64data1 <= i64data2;
			if(op == EXPR_GTE) i64data1 = i64data1 >= i64data2;
			if(op == EXPR_GT)  i64data1 = i64data1 > i64data2;
			if(op == EXPR_EQ)  i64data1 = i64data1 == i64data2;
			if(op == EXPR_NEQ) i64data1 = i64data1 != i64data2;
			memcpy(&vm_stack[vm_stackpointer-2].smalldata, &i64data1, 8);
			goto end_expr_lt;
		}

		/*case 1: both are u32*/
		if(
			vm_stack[vm_stackpointer-1].t.basetype == BASE_U32
		){
			memcpy(&u32data1, &vm_stack[vm_stackpointer-2].smalldata, 4);
			memcpy(&u32data2, &vm_stack[vm_stackpointer-1].smalldata, 4);
			if(op == EXPR_LT)  i64data1 = u32data1 < u32data2;
			if(op == EXPR_LTE) i64data1 = u32data1 <= u32data2;
			if(op == EXPR_GTE) i64data1 = u32data1 >= u32data2;
			if(op == EXPR_GT)  i64data1 = u32data1 > u32data2;
			if(op == EXPR_EQ)  i64data1 = u32data1 == u32data2;
			if(op == EXPR_NEQ) i64data1 = u32data1 != u32data2;
			memcpy(&vm_stack[vm_stackpointer-2].smalldata, &i64data1, 8);
			goto end_expr_lt;
		}		
		/*case 2: both are i32*/
		if(
			vm_stack[vm_stackpointer-1].t.basetype == BASE_I32
		){
			memcpy(&i32data1, &vm_stack[vm_stackpointer-2].smalldata, 4);
			memcpy(&i32data2, &vm_stack[vm_stackpointer-1].smalldata, 4);
			if(op == EXPR_LT)  i64data1 = i32data1 < i32data2;
			if(op == EXPR_LTE) i64data1 = i32data1 <= i32data2;
			if(op == EXPR_GTE) i64data1 = i32data1 >= i32data2;
			if(op == EXPR_GT)  i64data1 = i32data1 > i32data2;
			if(op == EXPR_EQ)  i64data1 = i32data1 == i32data2;
			if(op == EXPR_NEQ) i64data1 = i32data1 != i32data2;
			memcpy(&vm_stack[vm_stackpointer-2].smalldata, &i64data1, 8);
			goto end_expr_lt;
		}
		/*case 1: both are u16*/
		if(
			vm_stack[vm_stackpointer-1].t.basetype == BASE_U16
		){
			memcpy(&u16data1, &vm_stack[vm_stackpointer-2].smalldata, 2);
			memcpy(&u16data2, &vm_stack[vm_stackpointer-1].smalldata, 2);
			if(op == EXPR_LT)  i64data1 = u16data1 < u16data2;
			if(op == EXPR_LTE) i64data1 = u16data1 <= u16data2;
			if(op == EXPR_GTE) i64data1 = u16data1 >= u16data2;
			if(op == EXPR_GT)  i64data1 = u16data1 > u16data2;
			if(op == EXPR_EQ)  i64data1 = u16data1 == u16data2;
			if(op == EXPR_NEQ) i64data1 = u16data1 != u16data2;
			memcpy(&vm_stack[vm_stackpointer-2].smalldata, &i64data1, 8);
			goto end_expr_lt;
		}		
		/*case 2: both are i16*/
		if(
			vm_stack[vm_stackpointer-1].t.basetype == BASE_I16
		){
			memcpy(&i16data1, &vm_stack[vm_stackpointer-2].smalldata, 2);
			memcpy(&i16data2, &vm_stack[vm_stackpointer-1].smalldata, 2);
			if(op == EXPR_LT)  i64data1 = i16data1 < i16data2;
			if(op == EXPR_LTE) i64data1 = i16data1 <= i16data2;
			if(op == EXPR_GTE) i64data1 = i16data1 >= i16data2;
			if(op == EXPR_GT)  i64data1 = i16data1 > i16data2;
			if(op == EXPR_EQ)  i64data1 = i16data1 == i16data2;
			if(op == EXPR_NEQ) i64data1 = i16data1 != i16data2;
			memcpy(&vm_stack[vm_stackpointer-2].smalldata, &i64data1, 8);
			goto end_expr_lt;
		}		
		/*case 1: both are u8*/
		if(
			vm_stack[vm_stackpointer-1].t.basetype == BASE_U8
		){
			memcpy(&u8data1, &vm_stack[vm_stackpointer-2].smalldata, 1);
			memcpy(&u8data2, &vm_stack[vm_stackpointer-1].smalldata, 1);
			if(op == EXPR_LT)  i64data1 = u8data1 < u8data2;
			if(op == EXPR_LTE) i64data1 = u8data1 <= u8data2;
			if(op == EXPR_GTE) i64data1 = u8data1 >= u8data2;
			if(op == EXPR_GT)  i64data1 = u8data1 > u8data2;
			if(op == EXPR_EQ)  i64data1 = u8data1 == u8data2;
			if(op == EXPR_NEQ) i64data1 = u8data1 != u8data2;
			memcpy(&vm_stack[vm_stackpointer-2].smalldata, &i64data1, 8);
			goto end_expr_lt;
		}
		/*case 2: both are i8*/
		if(
			vm_stack[vm_stackpointer-1].t.basetype == BASE_I8
		){
			memcpy(&i8data1, &vm_stack[vm_stackpointer-2].smalldata, 1);
			memcpy(&i8data2, &vm_stack[vm_stackpointer-1].smalldata, 1);
			if(op == EXPR_LT)  i64data1 = i8data1 < i8data2;
			if(op == EXPR_LTE) i64data1 = i8data1 <= i8data2;
			if(op == EXPR_GTE) i64data1 = i8data1 >= i8data2;
			if(op == EXPR_GT)  i64data1 = i8data1 > i8data2;
			if(op == EXPR_EQ)  i64data1 = i8data1 == i8data2;
			if(op == EXPR_NEQ) i64data1 = i8data1 != i8data2;
			memcpy(&vm_stack[vm_stackpointer-2].smalldata, &i64data1, 8);
			goto end_expr_lt;
		}
		/*case 1: both are f32*/
		if(
			vm_stack[vm_stackpointer-1].t.basetype == BASE_F32
		){
			memcpy(&f32data1, &vm_stack[vm_stackpointer-2].smalldata, 4);
			memcpy(&f32data2, &vm_stack[vm_stackpointer-1].smalldata, 4);
			if(op == EXPR_LT)  i64data1 = f32data1 < f32data2;
			if(op == EXPR_LTE) i64data1 = f32data1 <= f32data2;
			if(op == EXPR_GTE) i64data1 = f32data1 >= f32data2;
			if(op == EXPR_GT)  i64data1 = f32data1 > f32data2;
			if(op == EXPR_EQ)  i64data1 = f32data1 == f32data2;
			if(op == EXPR_NEQ) i64data1 = f32data1 != f32data2;
			memcpy(&vm_stack[vm_stackpointer-2].smalldata, &i64data1, 8);
			goto end_expr_lt;
		}		/*case 1: both are f64*/
		if(
			vm_stack[vm_stackpointer-1].t.basetype == BASE_F64
		){
			memcpy(&f64data1, &vm_stack[vm_stackpointer-2].smalldata, 8);
			memcpy(&f64data2, &vm_stack[vm_stackpointer-1].smalldata, 8);
			if(op == EXPR_LT)  i64data1 = f64data1 < f64data2;
			if(op == EXPR_LTE) i64data1 = f64data1 <= f64data2;
			if(op == EXPR_GTE) i64data1 = f64data1 >= f64data2;
			if(op == EXPR_GT)  i64data1 = f64data1 > f64data2;
			if(op == EXPR_EQ)  i64data1 = f64data1 == f64data2;
			if(op == EXPR_NEQ) i64data1 = f64data1 != f64data2;
			memcpy(&vm_stack[vm_stackpointer-2].smalldata, &i64data1, 8);
			goto end_expr_lt;
		}
		puts("VM ERROR");
		puts("Unhandled lt/lte/gte/gt/==/!= type.");
		exit(1);
		end_expr_lt:;
		ast_vm_stack_pop(); //we no longer need the index itself.
		vm_stack[vm_stackpointer-1].t = ee->t;
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
			memcpy(
				&s, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				POINTER_SIZE
			);
			memcpy(
				&sz, 
				&vm_stack[vm_stackpointer-2].smalldata, 
				POINTER_SIZE
			);
			impl_builtin_emit(s,sz);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_getargc")){ //0 args
			int32_t v;
			v = impl_builtin_getargc();
			memcpy(
				&vm_stack[saved_vstack_pointer-1].smalldata, 
				&v, 
				4
			);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_getargv")){ //0 arguments
			char** v;
			v = impl_builtin_getargv();
			memcpy(
				&vm_stack[saved_vstack_pointer-1].smalldata, 
				&v, 
				POINTER_SIZE
			);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_malloc")){
			char** v;
			uint64_t sz;
			memcpy(
				&sz, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				8
			);
			v = impl_builtin_malloc(sz);
			memcpy(
				&vm_stack[saved_vstack_pointer-1].smalldata, 
				&v, 
				POINTER_SIZE
			);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_realloc")){
			char* v;
			uint64_t sz;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			memcpy(
				&v, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				POINTER_SIZE
			);
			memcpy(
				&sz, 
				&vm_stack[vm_stackpointer-2].smalldata, 
				8
			);
			v = impl_builtin_realloc(v,sz);
			
			memcpy(
				&vm_stack[saved_vstack_pointer-1].smalldata, 
				&v, 
				POINTER_SIZE
			);
			goto end_of_builtin_call;
		}		
		if(streq(ee->symname, "__builtin_type_getsz")){
			char* v;
			uint64_t sz;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			
			memcpy(
				&v, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				POINTER_SIZE
			);
			sz = impl_builtin_type_getsz(v);
			memcpy(
				&vm_stack[saved_vstack_pointer-1].smalldata, 
				&sz, 
				8
			);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_struct_metadata")){
			uint64_t sz;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			
			memcpy(
				&sz, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				8
			);
			sz = impl_builtin_struct_metadata(sz);
			memcpy(
				&vm_stack[saved_vstack_pointer-1].smalldata, 
				&sz, 
				8
			);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_strdup")){
			char* v;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			
			memcpy(
				&v, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				POINTER_SIZE
			);
			v = impl_builtin_strdup(v);
			
			memcpy(
				&vm_stack[saved_vstack_pointer-1].smalldata, 
				&v, 
				POINTER_SIZE
			);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_free")){
			char* v;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			
			memcpy(
				&v, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				POINTER_SIZE
			);
			impl_builtin_free(v);
			
			//memcpy(&vm_stack[saved_vstack_pointer-1].smalldata, &v, POINTER_SIZE);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_exit")){
			int32_t v;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			
			memcpy(
				&v, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				4
			);
			impl_builtin_exit(v);
			
			//memcpy(&vm_stack[saved_vstack_pointer-1].smalldata, &v, POINTER_SIZE);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_get_ast")){
			char* v;
			v = (char*) impl_builtin_get_ast();
			memcpy(
				&vm_stack[saved_vstack_pointer-1].smalldata, 
				&v, 
				POINTER_SIZE
			);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_peek")){
			char* v;
			v = (char*) impl_builtin_peek();
			
			memcpy(
				&vm_stack[saved_vstack_pointer-1].smalldata, 
				&v, 
				POINTER_SIZE
			);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_consume")){
			char* v;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			
			//memcpy(&v, &vm_stack[vm_stackpointer-1].smalldata, 4);
			v = (char*) impl_builtin_consume();
			
			memcpy(
				&vm_stack[saved_vstack_pointer-1].smalldata, 
				&v, 
				POINTER_SIZE
			);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_puts")){
			char* v;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			
			memcpy(
				&v, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				POINTER_SIZE
			);
			impl_builtin_puts(v);
			
			//memcpy(&vm_stack[saved_vstack_pointer-1].smalldata, &v, POINTER_SIZE);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_gets")){
			char* v;
			uint64_t sz;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			
			memcpy(
				&v, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				POINTER_SIZE
			);
			memcpy(
				&sz, 
				&vm_stack[vm_stackpointer-2].smalldata, 
				8
			);
			impl_builtin_gets(v,sz);
			
			//memcpy(&vm_stack[saved_vstack_pointer-1].smalldata, &v, POINTER_SIZE);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_open_ofile")){
			char* v;
			int32_t q;
			//remember: arguments are backwards on the stack! so the first argument is on top...
			
			memcpy(
				&v, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				POINTER_SIZE
			);
			//memcpy(&q, &vm_stack[vm_stackpointer-2].smalldata, 4);
			q = impl_builtin_open_ofile(v);
			memcpy(
				&vm_stack[saved_vstack_pointer-1].smalldata, 
				&q, 
				4
			);
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
		if(streq(ee->symname, "__builtin_memcpy")){
			char* a;
			char* b;
			uint64_t c;
			memcpy(&a, &vm_stack[vm_stackpointer-1].smalldata, POINTER_SIZE);
			memcpy(&b, &vm_stack[vm_stackpointer-2].smalldata, POINTER_SIZE);
			memcpy(&c, &vm_stack[vm_stackpointer-3].smalldata, 8);
			impl_builtin_memcpy(a,b,c);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_utoa")){
			char* buf;
			uint64_t i;
			memcpy(
				&buf, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				POINTER_SIZE
			);
			memcpy(
				&i, 
				&vm_stack[vm_stackpointer-2].smalldata, 
				8
			);
			impl_builtin_utoa(buf, i);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_itoa")){
			char* buf;
			int64_t i;
			memcpy(
				&buf, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				POINTER_SIZE
			);
			memcpy(
				&i, 
				&vm_stack[vm_stackpointer-2].smalldata, 
				8
			);
			impl_builtin_itoa(buf, i);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_ftoa")){
			char* buf;
			double i;
			memcpy(
				&buf, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				POINTER_SIZE
			);
			memcpy(
				&i, 
				&vm_stack[vm_stackpointer-2].smalldata, 
				8
			);
			impl_builtin_ftoa(buf, i);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_atof")){
			char* buf;
			double f;
			memcpy(
				&buf, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				POINTER_SIZE
			);
			f = impl_builtin_atof(buf);
			memcpy(
				&vm_stack[saved_vstack_pointer-1].smalldata, 
				&f, 
				8
			);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_atoi")){
			char* buf;
			int64_t f;
			memcpy(
				&buf, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				POINTER_SIZE
			);
			f = impl_builtin_atoi(buf);
			memcpy(
				&vm_stack[saved_vstack_pointer-1].smalldata, 
				&f, 
				8
			);
			goto end_of_builtin_call;
		}
		if(streq(ee->symname, "__builtin_atou")){
			char* buf;
			uint64_t f;
			memcpy(
				&buf, 
				&vm_stack[vm_stackpointer-1].smalldata, 
				POINTER_SIZE
			);
			f = impl_builtin_atou(buf);
			memcpy(
				&vm_stack[saved_vstack_pointer-1].smalldata, 
				&f, 
				8
			);
			goto end_of_builtin_call;
		}
		//TODO: implement more builtins.
		puts("VM Error");
		puts("Unhandled builtin call:");
		puts(ee->symname);
		exit(1);
		end_of_builtin_call:;
		for(i = 0; i < n_subexpressions; i++) {ast_vm_stack_pop();}
		return;
	}

	puts("VM error");
	puts("Unhandled expression node type.");
	exit(1);

}


typedef struct{
	uint64_t pos;
	uint64_t is_in_loop;
	uint64_t is_else_chaining;
}vm_scope_position_execution_info;


vm_scope_position_execution_info scope_positions[0x10000];

void ast_execute_function(symdecl* s){
	uint64_t which_stmt = 0;
	stmt* stmt_list = NULL;
	uint64_t stmt_kind = 0;
	/*are we just starting? The first function returns void.*/
	if(vm_stackpointer == 0){
		type t = type_init();
		ast_vm_stack_push_temporary(NULL,t);
		nscopes = 0;
	}

	begin_executing_function:
	{uint64_t i;
		for(i = 0; i < nsymbols; i++){
			if(s == symbol_table+i){
				active_function = i;
				goto found_our_boy;
			}
		}
		puts("VM error");
		puts("Could not find our boy");
		exit(1);
		found_our_boy:;
	}
	/*make sure that s is a function with a body.*/
	if(
		s->t.is_function == 0 ||
		s->fbody == NULL
	)
	{
		puts("VM Error");
		puts("Tried to execute incomplete or non-function symbol:");
		puts(s->name);
		exit(1);
	}
	/*Set up execution environment*/
	
	cur_expr_stack_usage = 0;
	cur_func_frame_size = 0;
	scopestack_push(s->fbody);


	begin_executing_scope:
		{
			uint64_t i;
			for(i = 0; i < scopestack_gettop()->nsyms; i++){
				ast_vm_stack_push_lvar(scopestack_gettop()->syms+i);
			}
			scope_positions[nscopes-1].pos = 0;
			scope_positions[nscopes-1].is_in_loop = 0;
			scope_positions[nscopes-1].is_else_chaining = 0;
			which_stmt = 0;
		}
	continue_executing_scope:
		while(1){
			stmt_list = scopestack_gettop()->stmts;
			if( (scopestack_gettop()->nstmts == 0) || (which_stmt >= scopestack_gettop()->nstmts) ){
				uint64_t i;
				scope_positions[nscopes-1].is_else_chaining = 0;
				if(scopestack_gettop() == s->fbody) {
					/*
						puts("VM WARNING:");
						puts("Function Body Fell Through during execution.");
						puts("This is undefined behavior- add a 'return' statement.");
						puts("Function:");
						puts(s->name);
					*/
					goto do_return_fallthrough;
				}
				//else, we have to clear our variables...
				ast_vm_stack_pop_temporaries();
				for(i = 0; i < scopestack_gettop()->nsyms;i++)
					ast_vm_stack_pop();
				//we have to remove ourselves.
				scopestack_pop();
				which_stmt = scope_positions[nscopes-1].pos;
				if(scope_positions[nscopes-1].is_in_loop == 0) which_stmt++; //if we are in a loop, we don't want to!
				continue;
			}
			/*
				TODO: Handle loops.
			*/
			stmt_kind = stmt_list[which_stmt].kind;
			if(
				stmt_kind == STMT_NOP || 
				stmt_kind == STMT_LABEL ||
				stmt_kind == STMT_ASM
			){
				goto do_next_stmt;
			}
			if(scope_positions[nscopes-1].is_else_chaining == 0)
				if(stmt_kind == STMT_ELSE || stmt_kind == STMT_ELIF){
					goto do_next_stmt;
				}
			//if is_else_chaining is set, but the statement is not an else or elif...
			if(scope_positions[nscopes-1].is_else_chaining > 0)
				if(stmt_kind != STMT_ELSE && stmt_kind != STMT_ELIF){
					scope_positions[nscopes-1].is_else_chaining = 0;
				}
			if(stmt_kind == STMT_BAD || stmt_kind >= NSTMT_TYPES){
				puts("VM ERROR:");
				puts("INVALID statement kind!");
				goto do_error;
			}
			if(stmt_kind == STMT_EXPR){
				do_expr(stmt_list[which_stmt].expressions[0]);
				ast_vm_stack_pop();
				goto do_next_stmt;
			}
			if(stmt_kind == STMT_GOTO){
				int64_t i;
				stmt* cur_stmt = stmt_list + which_stmt;
				for(i = 0; i < cur_stmt->goto_vardiff; i++){
					//debug_print("goto popping variables...",0,0);
					ast_vm_stack_pop();
				}
				for(i = 0; i < cur_stmt->goto_scopediff; i++ ){
					//debug_print("goto popping stacks...",0,0);
					scopestack_pop();
				}
				//find our new position...
				stmt_list = scopestack_gettop()->stmts;
				which_stmt = cur_stmt->goto_where_in_scope;
				goto continue_executing_scope;
			}
			if(stmt_kind == STMT_TAIL){
				int64_t i;
				stmt* cur_stmt = stmt_list + which_stmt;
				for(i = 0; i < cur_stmt->goto_vardiff; i++){
					//debug_print("goto popping variables...",0,0);
					ast_vm_stack_pop();
				}
				for(i = 0; i < cur_stmt->goto_scopediff; i++ ){
					//debug_print("goto popping stacks...",0,0);
					scopestack_pop();
				}
				for(i = 0; i < (int64_t)nsymbols; i++)
					if(streq(symbol_table[i].name, cur_stmt->referenced_label_name)){
						s = symbol_table + i;
						goto begin_executing_function;
					}
				puts("VM error");
				puts("Tail failed.");
				puts("Tail target was:");
				puts(cur_stmt->referenced_label_name);
				goto do_error;
			}
			if(stmt_kind == STMT_CONTINUE || stmt_kind == STMT_BREAK){
				int64_t i;
				stmt* cur_stmt = stmt_list + which_stmt;
				for(i = 0; i < cur_stmt->goto_vardiff; i++){
					//debug_print("goto popping variables...",0,0);
					ast_vm_stack_pop();
				}
				for(i = 0; i < cur_stmt->goto_scopediff; i++ ){
					//debug_print("goto popping stacks...",0,0);
					scopestack_pop();
				}
				//find our new position...
				stmt_list = scopestack_gettop()->stmts;
				for(i = 0; i <(int64_t) scopestack_gettop()->nstmts; i++)
					if(cur_stmt->referenced_loop == stmt_list+i){
						which_stmt = i;
						if(stmt_kind == STMT_CONTINUE)
							goto continue_executing_scope; //continues in the loop, is_in_loop is preserved.
						if(stmt_kind == STMT_BREAK){
							scope_positions[nscopes-1].is_in_loop = 0; //breaks out of the loop, is_in_loop is undone.
							goto do_next_stmt; //we do the very next statement.
						}
					}
				puts("VM error");
				puts("STMT_CONTINUE");
				goto do_error;
			}
			if(stmt_kind == STMT_SWITCH){
				int64_t val;int64_t i;char* name;
				stmt* cur_stmt;


				cur_stmt = stmt_list + which_stmt;
				do_expr(stmt_list[which_stmt].expressions[0]);
				val = vm_stack[vm_stackpointer-1].smalldata;
				if(val > (int64_t)cur_stmt->switch_nlabels || val < 0){
					puts("VM error");
					puts("Switch statement is beyond the bounds of its label list.");
					puts("Runtime code behavior is undefined (probably just crashes!), but you're protected inside the codegen VM.");
					puts("The function that caused this was:");
					puts(symbol_table[active_function].name);
					goto do_error;
				}
				ast_vm_stack_pop();
				//val %= cur_stmt->switch_nlabels;
				//perform a goto within the current scope.
				name = cur_stmt->switch_label_list[val];
				for(i = 0; i < (int64_t)cur_stmt->whereami->nstmts; i++){
					if(stmt_list[i].kind == STMT_LABEL)
						if(streq(name, stmt_list[i].referenced_label_name)){
							which_stmt = i;
							goto continue_executing_scope;
						}
				}
				goto do_next_stmt;
			}
			if(stmt_kind == STMT_RETURN){
				int64_t i;
				uint64_t retval;
				uint64_t has_retval = 0;
				stmt* cur_stmt = stmt_list + which_stmt;
				if((stmt_list[which_stmt].expressions[0])){
					has_retval = 1;
					do_expr(stmt_list[which_stmt].expressions[0]);
					retval = vm_stack[vm_stackpointer-1].smalldata;
					ast_vm_stack_pop();
				}
				for(i = 0; i < cur_stmt->goto_vardiff; i++){
					ast_vm_stack_pop();
				}
				for(i = 0; i < cur_stmt->goto_scopediff; i++ ){
					scopestack_pop();
				}
				if(has_retval){
					vm_stack[vm_stackpointer -1 - s->nargs].smalldata = retval;
				}
				goto do_return;
			}
			if(
				stmt_kind == STMT_WHILE ||
				stmt_kind == STMT_IF || 
				stmt_kind == STMT_ELIF
			){
				stmt* cur_stmt;
				//scope_positions[nscopes-1].is_else_chaining = 0;
				do_expr(stmt_list[which_stmt].expressions[0]);
				//debug_print("While or If got This from its expression: ",vm_stack[vm_stackpointer-1].smalldata,0);
				if(scope_positions[nscopes-1].is_else_chaining == 0)
					if(stmt_kind == STMT_ELIF)
						goto do_next_stmt;
				
				if(vm_stack[vm_stackpointer-1].smalldata){
					cur_stmt = stmt_list + which_stmt;
					//It has been decided. we are going to be executing this block.
					ast_vm_stack_pop();
					scope_positions[nscopes-1].pos = which_stmt;
					scope_positions[nscopes-1].is_else_chaining = 0;
					if(stmt_kind == STMT_WHILE) scope_positions[nscopes-1].is_in_loop = 1;
					else			scope_positions[nscopes-1].is_in_loop = 0;
					scopestack_push(cur_stmt->myscope);
					//while, if, and elif never chain elses if they execute their body.
					goto begin_executing_scope;
				} else {
					if(stmt_kind == STMT_IF || stmt_kind == STMT_ELIF)
						scope_positions[nscopes-1].is_else_chaining = 1;
					if(stmt_kind == STMT_WHILE) scope_positions[nscopes-1].is_in_loop = 0;
				}
				ast_vm_stack_pop();
				goto do_next_stmt;
			}
			if(scope_positions[nscopes-1].is_else_chaining > 0)
			if(stmt_kind == STMT_ELSE){
				stmt* cur_stmt;
				cur_stmt = stmt_list + which_stmt;
				//It has been decided. we are going to be executing this block.
				scope_positions[nscopes-1].pos = which_stmt;
				scopestack_push(cur_stmt->myscope);
				//else stops an else chain unconditionally.
				scope_positions[nscopes-1].is_else_chaining = 0;
				goto begin_executing_scope;
			}
			if(stmt_kind == STMT_FOR){
				stmt* cur_stmt;
				int64_t retval;
				//int64_t i;
				cur_stmt = stmt_list + which_stmt;
				if(scope_positions[nscopes-1].is_in_loop){
					//iterate.

					do_expr(stmt_list[which_stmt].expressions[2]);
					ast_vm_stack_pop();

					//comparison...
					do_expr(stmt_list[which_stmt].expressions[1]);
						retval = vm_stack[vm_stackpointer-1].smalldata;
					ast_vm_stack_pop();
					
					if(retval == 0){
						scope_positions[nscopes-1].is_in_loop = 0;
						goto do_next_stmt;
					}
					// we are still doing it!
					scope_positions[nscopes-1].pos = which_stmt;
					scopestack_push(cur_stmt->myscope);
					goto begin_executing_scope;
				}
				//Do the initialization expression.
				do_expr(stmt_list[which_stmt].expressions[0]);
				//throw the value away!
				ast_vm_stack_pop();
				//do the second expression...
				do_expr(stmt_list[which_stmt].expressions[1]);
				//get that value.
				retval = vm_stack[vm_stackpointer-1].smalldata;
				ast_vm_stack_pop();
				scope_positions[nscopes-1].pos = which_stmt;
				if(retval == 0){
					scope_positions[nscopes-1].is_in_loop = 0;
					goto do_next_stmt;
				}
				scope_positions[nscopes-1].is_in_loop = 1;
				scopestack_push(cur_stmt->myscope);
				goto begin_executing_scope;
			}

		
			do_next_stmt:
			which_stmt++;
			continue;
		}


	do_return:
		ast_vm_stack_pop_temporaries();
		ast_vm_stack_pop_lvars();
		return;

	do_tail:
		//TODO: figure out how to do tail.
		ast_vm_stack_pop_temporaries();
		ast_vm_stack_pop_lvars();
		return;

	do_error:
	puts("While executing function:");
	puts(s->name);
	exit(1);
	do_return_fallthrough:{
		uint64_t i;
		//clear our variables.
		ast_vm_stack_pop_temporaries();
		for(i = 0; i < scopestack_gettop()->nsyms;i++) ast_vm_stack_pop();
		//we have to remove ourselves.
		scopestack_pop();
	}
}


