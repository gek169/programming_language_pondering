


#include "stringutil.h"
#include "targspecifics.h"

#include "libmin.h"
#include "parser.h"
#include "data.h"
#include "metaprogramming.h"
#include "astexec.h"

static ast_vm_stack_elem vm_stack[0x10000];
static uint64_t vm_stackpointer = 0;


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

static int64_t do_imod(int64_t a, int64_t b){return a % b;}
static uint64_t do_umod(uint64_t a, uint64_t b){return a % b;}

static int64_t do_lsh(int64_t a, int64_t b){return a<<b;}
static int64_t do_rsh(int64_t a, int64_t b){return a>>b;}
static int64_t do_bitand(int64_t a, int64_t b){return a&b;}
static int64_t do_bitor(int64_t a, int64_t b){return a|b;}
static int64_t do_bitxor(int64_t a, int64_t b){return a ^ b;}
static int64_t do_logand(int64_t a, int64_t b){return a && b;}
static int64_t do_logor(int64_t a, int64_t b){return a || b;}

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

