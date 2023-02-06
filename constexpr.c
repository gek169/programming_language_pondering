
#include "constexpr.h"

#include "targspecifics.h"

#include "libmin.h"


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

static int64_t cexpr_int_parse_paren(){
	int64_t a;
	if(peek()->data == TOK_INT_CONST) return cexpr_int_parse_int();
	require(peek()->data == TOK_OPAREN, "Integer constexpr expected opening parentheses or integer...");
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
	require(peek()->data == TOK_OPAREN, "Float constexpr expected opening parentheses or integer...");
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

