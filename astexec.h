#include <stdint.h>

enum{
	VM_VARIABLE,
	VM_FFRAME_BEGIN,
	VM_EXPRESSION_TEMPORARY
};


typedef struct{
	uint64_t identification;
	type t;
	uint64_t smalldata;
	uint8_t* ldata; /*for arrays and structs.*/
	char* vname; /*When a variable is pushed, a non-owning pointer to its name is written here.*/
}ast_vm_stack_elem;



void ast_execute_function(symdecl* s); //execute function in the AST VM.


