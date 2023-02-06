#include <stdint.h>

enum{
	VM_VARIABLE,
	VM_FARGS_BEGIN,
	VM_FFRAME_BEGIN,
	VM_EXPRESSION_TEMPORARY
};


typedef struct{
	uint64_t identification;
	type t;
	uint64_t smalldata;
	uint8_t* ldata; /*for arrays and structs.*/
}ast_vm_stack_elem;


uint64_t determine_fn_variable_stack_placements(symdecl* s);


