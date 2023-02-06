
#include "constexpr.h"

#include "targspecifics.h"

#include "libmin.h"
#include "data.h"
#include "parser.h"
/*
	TODO: Implement way for constant expressions to be executed by 

*/


static int64_t cexpr_int_parse_int(){
	int64_t retval;
	require(peek()->data == TOK_INT_CONST, "Integer Constant expression parsing expected integer constant...");
	retval = matou(peek()->text);
	consume();
	//puts("Parsed Int!");
	return retval;
}
static double cexpr_double_parse_double(){
	double retval;
	require(

		peek()->data == TOK_FLOAT_CONST
		||
		peek()->data == TOK_INT_CONST
		, "Float Constant expression parsing expected constant...");
	if(peek()->data == TOK_FLOAT_CONST)
		retval = matof(peek()->text);
	/*allow integers to be parsed as well.*/
	if(peek()->data == TOK_INT_CONST)
		retval = matou(peek()->text);
	consume();
	return retval;
}


static int64_t cexpr_int_parse_ident(){
	uint64_t symid;
	uint64_t i;
	int64_t retval;
	double f64_data;
	float f32_data;
	int32_t i32data;
	uint32_t u32data;
	int16_t i16data;
	uint16_t u16data;
	int8_t i8data;
	uint8_t u8data;
	int found;
	found = 0;
	require(peek()->data == TOK_IDENT, "cexpr_parse_ident: Required Identifier");
	require(!peek_is_typename(), "cexpr_parse_ident: Fatal: identifier is typename");
	require(peek_ident_is_already_used_globally(), "Identifier in const expression is not in use!");
	for(i = 0; i < nsymbols; i++){
		if(streq(peek()->text, symbol_table[i].name))
		{
			found = 1;
			symid = i;
			if(symbol_table[i].is_incomplete)
				parse_error("Attempted to use incomplete symbol in constant expression.");
			if(symbol_table[i].is_codegen == 0)
				parse_error("Attempted to use non-codegen identifier in constant expression.");
			if(symbol_table[i].t.basetype == BASE_STRUCT)
				parse_error("Attempted to use struct variable in constant expression.");
			if(symbol_table[i].cdata == NULL)
				parse_error("Attempted to use un-initialized identifier in constant expression.");
			if(symbol_table[i].t.arraylen > 0)
				parse_error("Attempted to use array identifier in constant expression.");
		}
	}
	if(!found) parse_error("Unknown identifier in constant expression.");
	consume(); //eat the identifier.
	if(symbol_table[symid].t.pointerlevel > 0){
		require(symbol_table[symid].cdata_sz == POINTER_SIZE, "Constexpr Identifier cdata size error.");
		memcpy(&retval, symbol_table[symid].cdata, POINTER_SIZE);
		
		return retval;
	}
	if(symbol_table[symid].t.basetype == BASE_U64 ||
	symbol_table[symid].t.basetype == BASE_I64	){
		require(symbol_table[symid].cdata_sz == 8, "Constexpr Identifier cdata size error.");
		memcpy(&retval, symbol_table[symid].cdata, 8);
		return retval;
	}
	if(symbol_table[symid].t.basetype == BASE_I32){
		require(symbol_table[symid].cdata_sz == 4, "Constexpr Identifier cdata size error.");
		memcpy(&i32data, symbol_table[symid].cdata, 4);
		retval = i32data;
		return retval;
	}
	if(symbol_table[symid].t.basetype == BASE_I16){
		require(symbol_table[symid].cdata_sz == 2, "Constexpr Identifier cdata size error.");
		memcpy(&i16data, symbol_table[symid].cdata, 2);
		retval = i16data;
		return retval;
	}
	if(symbol_table[symid].t.basetype == BASE_I8){
		require(symbol_table[symid].cdata_sz == 1, "Constexpr Identifier cdata size error.");
		memcpy(&i16data, symbol_table[symid].cdata, 1);
		retval = i8data;
		return retval;
	}
	if(symbol_table[symid].t.basetype == BASE_U32){
		require(symbol_table[symid].cdata_sz == 4, "Constexpr Identifier cdata size error.");
		memcpy(&u32data, symbol_table[symid].cdata, 4);
		retval = (uint64_t)u32data;
		return retval;
	}
	if(symbol_table[symid].t.basetype == BASE_U16){
		require(symbol_table[symid].cdata_sz == 2, "Constexpr Identifier cdata size error.");
		memcpy(&u16data, symbol_table[symid].cdata, 2);
		retval = (uint64_t)u16data;
		return retval;
	}
	if(symbol_table[symid].t.basetype == BASE_U8){
		require(symbol_table[symid].cdata_sz == 1, "Constexpr Identifier cdata size error.");
		memcpy(&u8data, symbol_table[symid].cdata, 1);
		retval = (uint64_t)u8data;
		return retval;
	}
	if(symbol_table[symid].t.basetype == BASE_F32){
		require(symbol_table[symid].cdata_sz == 4, "Constexpr Identifier cdata size error.");
		memcpy(&f32_data, symbol_table[symid].cdata, 4);
		retval = f32_data;
		return retval;
	}
	if(symbol_table[symid].t.basetype == BASE_F64){
		require(symbol_table[symid].cdata_sz == 8, "Constexpr Identifier cdata size error.");
		memcpy(&f64_data, symbol_table[symid].cdata, 8);
		retval = f32_data;
		return retval;
	}
	parse_error("Unhandled type in cexpr_int_parse_ident");
}



static double cexpr_double_parse_ident(){
	uint64_t symid;
	uint64_t i;
	int64_t i64_data;
	double f64_data;
	float f32_data;
	int32_t i32data;
	uint32_t u32data;
	int16_t i16data;
	uint16_t u16data;
	int8_t i8data;
	uint8_t u8data;
	int found;
	found = 0;
	require(peek()->data == TOK_IDENT, "cexpr_parse_ident: Required Identifier");
	require(!peek_is_typename(), "cexpr_parse_ident: Fatal: identifier is typename");
	require(peek_ident_is_already_used_globally(), "Identifier in const expression is not in use!");
	for(i = 0; i < nsymbols; i++){
		if(streq(peek()->text, symbol_table[i].name))
		{
			found = 1;
			symid = i;
			if(symbol_table[i].is_incomplete)
				parse_error("Attempted to use incomplete symbol in constant expression.");
			if(symbol_table[i].is_codegen == 0)
				parse_error("Attempted to use non-codegen identifier in constant expression.");
			if(symbol_table[i].t.basetype == BASE_STRUCT)
				parse_error("Attempted to use struct variable in constant expression.");
			if(symbol_table[i].cdata == NULL)
				parse_error("Attempted to use un-initialized identifier in constant expression.");
			if(symbol_table[i].t.arraylen > 0)
				parse_error("Attempted to use array identifier in constant expression.");
		}
	}
	if(!found) parse_error("Unknown identifier in constant expression.");
	consume(); //eat the identifier.
	if(symbol_table[symid].t.pointerlevel > 0){
		parse_error("Cannot use pointer identifier in double constexpr.");
	}
	if(symbol_table[symid].t.basetype == BASE_U64 ||
	symbol_table[symid].t.basetype == BASE_I64	){
		require(symbol_table[symid].cdata_sz == 8, "Constexpr Identifier cdata size error.");
		memcpy(&i64_data, symbol_table[symid].cdata, 8);
		f64_data = i64_data;
		return f64_data;
	}
	if(symbol_table[symid].t.basetype == BASE_I32){
		require(symbol_table[symid].cdata_sz == 4, "Constexpr Identifier cdata size error.");
		memcpy(&i32data, symbol_table[symid].cdata, 4);
		f64_data = i32data;
		return f64_data;
	}
	if(symbol_table[symid].t.basetype == BASE_I16){
		require(symbol_table[symid].cdata_sz == 2, "Constexpr Identifier cdata size error.");
		memcpy(&i16data, symbol_table[symid].cdata, 2);
		f64_data = i16data;
		return f64_data;
	}
	if(symbol_table[symid].t.basetype == BASE_I8){
		require(symbol_table[symid].cdata_sz == 1, "Constexpr Identifier cdata size error.");
		memcpy(&i16data, symbol_table[symid].cdata, 1);
		f64_data = i8data;
		return f64_data;
	}
	if(symbol_table[symid].t.basetype == BASE_U32){
		require(symbol_table[symid].cdata_sz == 4, "Constexpr Identifier cdata size error.");
		memcpy(&u32data, symbol_table[symid].cdata, 4);
		f64_data = (uint64_t)u32data;
		return f64_data;
	}
	if(symbol_table[symid].t.basetype == BASE_U16){
		require(symbol_table[symid].cdata_sz == 2, "Constexpr Identifier cdata size error.");
		memcpy(&u16data, symbol_table[symid].cdata, 2);
		f64_data = (uint64_t)u16data;
		return f64_data;
	}
	if(symbol_table[symid].t.basetype == BASE_U8){
		require(symbol_table[symid].cdata_sz == 1, "Constexpr Identifier cdata size error.");
		memcpy(&u8data, symbol_table[symid].cdata, 1);
		f64_data = (uint64_t)u8data;
		return f64_data;
	}
	if(symbol_table[symid].t.basetype == BASE_F32){
		require(symbol_table[symid].cdata_sz == 4, "Constexpr Identifier cdata size error.");
		memcpy(&f32_data, symbol_table[symid].cdata, 4);
		f64_data = f32_data;
		return f64_data;
	}
	if(symbol_table[symid].t.basetype == BASE_F64){
		require(symbol_table[symid].cdata_sz == 8, "Constexpr Identifier cdata size error.");
		memcpy(&f64_data, symbol_table[symid].cdata, 8);
		return f64_data;
	}
	parse_error("Unhandled type in cexpr_double_parse_ident");
}


/*
	TODO: This is the point where we would insert the ability to reference codegen functions
	and variables.
*/

static int64_t cexpr_int_parse_paren(){
	int64_t a;
	if(peek()->data == TOK_INT_CONST) return cexpr_int_parse_int();
	if(peek()->data == TOK_FLOAT_CONST) return cexpr_double_parse_double();
	if(peek()->data == TOK_IDENT){
		if(!peek_is_fname())
			return cexpr_int_parse_ident();
	}
	require(peek()->data == TOK_OPAREN, "Integer constexpr expected opening parentheses, float, ident, or integer...");
	consume();
	a = parse_cexpr_int();
	require(peek()->data == TOK_CPAREN, "Integer constexpr expected closing parentheses...");
	consume();
	return a;
}
static double cexpr_double_parse_paren(){
	double a;
	if(
		peek()->data == TOK_INT_CONST ||
		peek()->data == TOK_FLOAT_CONST
	) return cexpr_double_parse_double();
	if(peek()->data == TOK_IDENT){
		if(!peek_is_fname())
			return cexpr_double_parse_ident();
	}

	require(peek()->data == TOK_OPAREN, "Float constexpr expected opening parentheses, float, identifier, integer...");
	consume();
	a = parse_cexpr_double();
	require(peek()->data == TOK_CPAREN, "Float constexpr expected closing parentheses...");
	consume();
	return a;
}

static int64_t cexpr_int_parse_postfix(){
	int64_t a;
	a = cexpr_int_parse_paren();
	while(peek()->data == TOK_OPERATOR){
		if(streq(peek()->text,"++")){ /*incr*/
			consume();
			a++;
			continue;
		}
		if(streq(peek()->text,"--")){ /*decr*/
			consume();
			a--;
			continue;
		}
		break;
	}
	return a;
}
static double cexpr_double_parse_postfix(){
	double a;
	a = cexpr_double_parse_paren();
	while(peek()->data == TOK_OPERATOR){
		if(streq(peek()->text,"++")){ /*incr*/
			consume();
			a = a + 1;
			continue;
		}
		if(streq(peek()->text,"--")){ /*decr*/
			consume();
			a = a - 1;
			continue;
		}
		break;
	}
	return a;
}

static int64_t cexpr_int_parse_prefix_expr(){
	if(peek()->data == TOK_OPERATOR){
		if(streq(peek()->text,"-")){
			consume();
			return -1 * cexpr_int_parse_prefix_expr();
		}
		if(streq(peek()->text,"~")){ /*compl*/
			consume();
			return ~cexpr_int_parse_prefix_expr();
		}
		if(streq(peek()->text,"!")){ /*compl*/
			consume();
			return !cexpr_int_parse_prefix_expr();
		}		
		if(streq(peek()->text,"++")){ /*compl*/
			consume();
			return 1+cexpr_int_parse_prefix_expr();
		}
		if(streq(peek()->text,"--")){ /*compl*/
			consume();
			return cexpr_int_parse_prefix_expr()-1;
		}
	}
	return cexpr_int_parse_postfix(); /*TODO: postfix expression, ++ and --*/
}
static double cexpr_double_parse_prefix_expr(){
	if(peek()->data == TOK_OPERATOR){
		if(streq(peek()->text,"-")){
			consume();
			return -1 * cexpr_double_parse_prefix_expr();
		}
		if(streq(peek()->text,"~")){ /*compl*/
			consume();
			return ~(int64_t)cexpr_double_parse_prefix_expr();
		}
		if(streq(peek()->text,"!")){ /*compl*/
			consume();
			return !(int64_t)cexpr_double_parse_prefix_expr();
		}		
		if(streq(peek()->text,"++")){ /*compl*/
			consume();
			return 1+cexpr_double_parse_prefix_expr();
		}
		if(streq(peek()->text,"--")){ /*compl*/
			consume();
			return cexpr_double_parse_prefix_expr()-1;
		}
	}
	return cexpr_double_parse_postfix(); /*TODO: postfix expression, ++ and --*/
}

static int64_t cexpr_int_parse_muldiv_expr(){
	int64_t a;
	int64_t b;
	a = cexpr_int_parse_prefix_expr();
	while(1){
		if(peek()->data == TOK_OPERATOR){
			if(streq(peek()->text,"*")){
				consume();
				b = cexpr_int_parse_prefix_expr();
				a *= b;
				continue;
			}
			if(streq(peek()->text,"/")){
				consume();
				b = cexpr_int_parse_prefix_expr();
				require(b!=0,"Compiletime divide by zero in constexpr.");
				a /= b;
				continue;
			}
			if(streq(peek()->text,"%")){
				consume();
				b = cexpr_int_parse_prefix_expr();
				require(b!=0,"Compiletime divide by zero in constexpr.");
				a %= b;
				continue;
			}
		}
		break;
	}
	return a;
}

static double cexpr_double_parse_muldiv_expr(){
	double a;
	double b;
	a = cexpr_double_parse_prefix_expr();
	while(1){
		if(peek()->data == TOK_OPERATOR){
			if(streq(peek()->text,"*")){
				consume();
				b = cexpr_double_parse_prefix_expr();
				a *= b;
				continue;
			}
			if(streq(peek()->text,"/")){
				consume();
				b = cexpr_double_parse_prefix_expr();
				require(b!=0,"Compiletime divide by zero in constexpr.");
				a /= b;
				continue;
			}
		}
		break;
	}
	return a;
}


static int64_t cexpr_int_parse_addsub_expr(){
	int64_t a;
	int64_t b;
	a = cexpr_int_parse_muldiv_expr();
	while(1){
		if(peek()->data == TOK_OPERATOR){
			if(streq(peek()->text,"+")){
				consume();
				b = cexpr_int_parse_muldiv_expr();
				a += b;
				continue;
			}
			if(streq(peek()->text,"-")){
				consume();
				b = cexpr_int_parse_muldiv_expr();
				a -= b;
				continue;
			}
		}
		break;
	}
	return a;
}
static double cexpr_double_parse_addsub_expr(){
	double a;
	double b;
	a = cexpr_double_parse_muldiv_expr();
	while(1){
		if(peek()->data == TOK_OPERATOR){
			if(streq(peek()->text,"+")){
				consume();
				b = cexpr_double_parse_muldiv_expr();
				a += b;
				continue;
			}
			if(streq(peek()->text,"-")){
				consume();
				b = cexpr_double_parse_muldiv_expr();
				a -= b;
				continue;
			}
		}
		break;
	}
	return a;
}

static int64_t cexpr_int_parse_bit_expr(){
	int64_t a;
	int64_t b;
	a = cexpr_int_parse_addsub_expr();
	while(1){
		if(peek()->data == TOK_OPERATOR){
			if(streq(peek()->text,"|")){
				consume();
				b = cexpr_int_parse_addsub_expr();
				a |= b;
				continue;
			}
			if(streq(peek()->text,"&")){
				consume();
				b = cexpr_int_parse_addsub_expr();
				a &= b;
				continue;
			}
			if(streq(peek()->text,"^")){
				consume();
				b = cexpr_int_parse_addsub_expr();
				a = a ^ b;
				continue;
			}
			if(streq(peek()->text,"<<")){
				consume();
				b = cexpr_int_parse_addsub_expr();
				a = a << b;
				continue;
			}
			if(streq(peek()->text,">>")){
				consume();
				b = cexpr_int_parse_addsub_expr();
				a = a >> b;
				continue;
			}
		}
		break;
	}
	return a;
}
static double cexpr_double_parse_bit_expr(){
	double a;
	double b;
	a = cexpr_double_parse_addsub_expr();
	while(1){
		if(peek()->data == TOK_OPERATOR){
			if(streq(peek()->text,"|")){
				consume();
				b = cexpr_double_parse_addsub_expr();
				a = (int64_t)a | (int64_t)b;
				continue;
			}
			if(streq(peek()->text,"&")){
				consume();
				b = cexpr_double_parse_addsub_expr();
				a = (int64_t)a & (int64_t)b;
				continue;
			}
			if(streq(peek()->text,"^")){
				consume();
				b = cexpr_double_parse_addsub_expr();
				a = (int64_t)a ^ (int64_t)b;
				continue;
			}
			if(streq(peek()->text,"<<")){
				consume();
				b = cexpr_double_parse_addsub_expr();
				a = (int64_t)a << (int64_t)b;
				continue;
			}
			if(streq(peek()->text,">>")){
				consume();
				b = cexpr_double_parse_addsub_expr();
				a = (int64_t)a >> (int64_t)b;
				continue;
			}
		}
		break;
	}
	return a;
}



static int64_t cexpr_int_parse_compare_expr(){
	int64_t a;
	int64_t b;
	a = cexpr_int_parse_bit_expr();
	while(1){
		if(peek()->data == TOK_OPERATOR){
			if(streq(peek()->text,"<")){
				consume();
				b = cexpr_int_parse_bit_expr();
				a = (a < b);
				continue;
			}
			if(streq(peek()->text,">")){
				consume();
				b = cexpr_int_parse_bit_expr();
				a = (a > b);
				continue;
			}			
			if(streq(peek()->text,"<=")){
				consume();
				b = cexpr_int_parse_bit_expr();
				a = (a <= b);
				continue;
			}
			if(streq(peek()->text,">=")){
				consume();
				b = cexpr_int_parse_bit_expr();
				a = (a >= b);
				continue;
			}			
			if(streq(peek()->text,"==")){
				consume();
				b = cexpr_int_parse_bit_expr();
				a = (a == b);
				continue;
			}
			if(streq(peek()->text,"!=")){
				consume();
				b = cexpr_int_parse_bit_expr();
				a = (a != b);
				continue;
			}
		}
		break;
	}
	return a;
}
static double cexpr_double_parse_compare_expr(){
	double a;
	double b;
	a = cexpr_double_parse_bit_expr();
	while(1){
		if(peek()->data == TOK_OPERATOR){
			if(streq(peek()->text,"<")){
				consume();
				b = cexpr_double_parse_bit_expr();
				a = (int64_t)(a < b);
				continue;
			}
			if(streq(peek()->text,">")){
				consume();
				b = cexpr_double_parse_bit_expr();
				a = (int64_t)(a > b);
				continue;
			}			
			if(streq(peek()->text,"<=")){
				consume();
				b = cexpr_double_parse_bit_expr();
				a = (int64_t)(a <= b);
				continue;
			}
			if(streq(peek()->text,">=")){
				consume();
				b = cexpr_double_parse_bit_expr();
				a = (int64_t)(a >= b);
				continue;
			}			
			if(streq(peek()->text,"==")){
				consume();
				b = cexpr_double_parse_bit_expr();
				a = (int64_t)(a == b);
				continue;
			}
			if(streq(peek()->text,"!=")){
				consume();
				b = cexpr_double_parse_bit_expr();
				a = (int64_t)(a != b);
				continue;
			}
		}
		break;
	}
	return a;
}


static int64_t cexpr_int_parse_logbool_expr(){
	int64_t a;
	int64_t b;
	a = cexpr_int_parse_compare_expr();
	while(1){
		if(peek()->data == TOK_OPERATOR){
			if(streq(peek()->text,"||")){
				consume();
				b = cexpr_int_parse_compare_expr();
				a = a || b;
				continue;
			}
			if(streq(peek()->text,"&&")){
				consume();
				b = cexpr_int_parse_compare_expr();
				a = a && b;
				continue;
			}
		}
		break;
	}
	return a;
}
static double cexpr_double_parse_logbool_expr(){
	double a;
	double b;
	a = cexpr_double_parse_compare_expr();
	while(1){
		if(peek()->data == TOK_OPERATOR){
			if(streq(peek()->text,"||")){
				consume();
				b = cexpr_double_parse_compare_expr();
				a = (a!=0.0) || (b!=0.0);
				continue;
			}
			if(streq(peek()->text,"&&")){
				consume();
				b = cexpr_double_parse_compare_expr();
				a = (a!=0.0) && (b!=0.0);
				continue;
			}
		}
		break;
	}
	return a;
}



int64_t parse_cexpr_int(){
	return cexpr_int_parse_compare_expr();
}
double parse_cexpr_double(){
	return cexpr_double_parse_compare_expr();
}

