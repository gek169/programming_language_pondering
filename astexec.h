#include <stdint.h>

enum{
	VM_VARIABLE,
	VM_EXPRESSION_TEMPORARY
};


typedef struct{
	uint64_t identification;
	type t;
	uint64_t smalldata;
	uint8_t* ldata; /*for arrays and structs.*/
}ast_vm_stack_elem;



void ast_execute_function(symdecl* s); //execute function in the AST VM.
void ast_execute_expr_parsehook(symdecl* s, expr_node** targ);

