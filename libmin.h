/*LIBMIN
	C89 minimalist library to replace the libc.

	Used for programs that do not use files.
	
	include libmin.h into your program today!
*/


#ifndef NULL
#define NULL ((void*)0)
#endif


#ifndef LIBMIN_UINT
#define LIBMIN_UINT unsigned long
#endif


#ifndef LIBMIN_INT
#define LIBMIN_INT long
#endif

#ifndef LIBMIN_FLOAT
#define LIBMIN_FLOAT float
#endif




/*Libmin include mode!*/

#ifndef LIBMIN_MODE_HEADER
#ifndef LIBMIN_MODE_C_FILE
#define LIBMIN_MODE_STATIC_HEADER
#endif
#endif

#ifdef LIBMIN_MODE_STATIC_HEADER
#ifdef LIBMIN_MODE_HEADER
__this_will_not_compile
#endif
#endif

#ifdef LIBMIN_MODE_STATIC_HEADER
#ifdef LIBMIN_MODE_C_FILE
__this_will_not_compile
#endif
#endif


#ifdef LIBMIN_MODE_HEADER
#ifdef LIBMIN_MODE_C_FILE
__this_will_not_compile
#endif
#endif



#ifdef LIBMIN_MODE_STATIC_HEADER
#define LIBMIN_FUNC_ATTRIBS static
#define LIBMIN_GVAR_ATTRIBS static
#else
#define LIBMIN_FUNC_ATTRIBS /*a comment*/
#define LIBMIN_GVAR_ATTRIBS /*a comment*/
#endif

#ifdef LIBMIN_MODE_HEADER

LIBMIN_FUNC_ATTRIBS LIBMIN_UINT mstrlen(const char* s);
#else
LIBMIN_FUNC_ATTRIBS LIBMIN_UINT mstrlen(const char* s){
	LIBMIN_UINT q; q=0;
	while(*s != '\0'){
		s++;q++;
	}
	return q;
}
#endif




#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS void mstrcpy(char* d, const char* s);
#else
LIBMIN_FUNC_ATTRIBS void mstrcpy(char* d, const char* s){
	while(*s != '\0')
	{
		*d = *s;
		s++;
		d++;
	}
	*d = '\0';
}
#endif

#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS LIBMIN_INT mstrcmp(char* d, const char* s);
#else
LIBMIN_FUNC_ATTRIBS LIBMIN_INT mstrcmp(char* d, const char* s){
	while(*s == *d)
	{
		if(*s == '\0') return 0;
		s++;
		d++;
	}
	return (LIBMIN_INT)(*d) - (LIBMIN_INT)(*s);
}
#endif


#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS void mstrncpy(char* d, const char* s, LIBMIN_UINT n);
#else
LIBMIN_FUNC_ATTRIBS void mstrncpy(char* d, const char* s, LIBMIN_UINT n){
	while((*s != '\0') && n){
		*d=*s;
		s++;
		d++;
		n--;
	}
	if(n)*d = 0;
}
#endif



#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS void mstrncat(char* d, const char* s, LIBMIN_UINT n);
#else
LIBMIN_FUNC_ATTRIBS void mstrncat(char* d, const char* s, LIBMIN_UINT n){
	while(*d && n){
		d++;n--;
	}
	if(n==0) return;
	mstrncpy(d,s,n);
}
#endif

#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS void mstrcat(char* d, const char* s);
#else
LIBMIN_FUNC_ATTRIBS void mstrcat(char* d, const char* s){
	while(*d){d++;}
	/*reached null terminator*/
	mstrcpy(d,s);
}
#endif

#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS void mmemcpy(char* d, const char* s, LIBMIN_UINT size);
#else
LIBMIN_FUNC_ATTRIBS void mmemcpy(char* d, const char* s, LIBMIN_UINT size){
	while(size){
		*d = *s;
		s++;d++;size--;
	}
}
#endif


#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS void mmemset(char* d, const char v, LIBMIN_UINT size);
#else
LIBMIN_FUNC_ATTRIBS void mmemset(char* d, const char v, LIBMIN_UINT size){
	while(size){
		*d = v;
		d++;size--;
	}
}
#endif


#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS char misdigit(const char s);
#else
LIBMIN_FUNC_ATTRIBS char misdigit(const char s){
	if(s >= '0')
		if(s <= '9')
			return 1;
	return 0;
}
#endif


#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS char misoct(const char s);
#else
LIBMIN_FUNC_ATTRIBS char misoct(const char s){
	if(s >= '0')
		if(s <= '7')
			return 1;
	return 0;
}
#endif

#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS char mishex(const char s);
#else
LIBMIN_FUNC_ATTRIBS char mishex(const char s){
	if(misdigit(s) ||
		((s<='f') && s>='a') ||
		((s<='F') && s>='A')
	)
	return 1;
	return 0;
}
#endif




#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS LIBMIN_INT mabs(LIBMIN_INT a);
#else
LIBMIN_FUNC_ATTRIBS LIBMIN_INT mabs(LIBMIN_INT a){
	if(a >= 0) return a;
	else return -a;
}
#endif


#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS LIBMIN_UINT matou(const char* s);
#else
LIBMIN_FUNC_ATTRIBS LIBMIN_UINT matou(const char* s){
	LIBMIN_UINT retval;
	retval = 0;
	/*goto has a place in 2022*/
	if(*s == '\0') return 0; /*no!*/
	if(*s == '0') goto octal;
	goto decimal;

	
	octal:s++;
	if(*s == 'x' || *s == 'X') goto hex;
	while(*s && misoct(*s)){
		retval *= 8;
		retval += (LIBMIN_UINT)((*s) - ('0'));
		s++;
	}
	return retval;


	hex:s++;
	while(*s && mishex(*s)){
		retval *= 16;
		switch(*s){
			
			default: return retval;
			case 'a':case 'A': retval += 10; break;
			case 'b':case 'B': retval += 11; break;
			case 'c':case 'C': retval += 12; break;
			case 'd':case 'D': retval += 13; break;
			case 'e':case 'E': retval += 14; break;
			case 'f':case 'F': retval += 15; break;
			case '0': break;
			case '1': retval+=1; break;
			case '2': retval+=2; break;
			case '3': retval+=3; break;
			case '4': retval+=4; break;
			case '5': retval+=5; break;
			case '7': retval+=7; break;
			case '8': retval+=8; break;
			case '9': retval+=9; break;
		}
		s++;
	}
	return retval;



	decimal:;
	while(*s && misdigit(*s)){
		retval *= 10;
		retval += (LIBMIN_UINT)((*s) - ('0'));
		s++;
	}
	return retval;
}
#endif




#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS LIBMIN_INT matoi(char*s);
#else
LIBMIN_FUNC_ATTRIBS LIBMIN_INT matoi(char*s){
	LIBMIN_INT retval = 0;
	if(*s == '+') retval = (LIBMIN_INT)matou(s+1);
	else if(*s == '-') retval = ((LIBMIN_INT)matou(s+1)) * -1;
	else return matou(s);
	return retval;
}
#endif


#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS void mutoa(char* dest, LIBMIN_UINT value);
#else
LIBMIN_FUNC_ATTRIBS void mutoa(char* dest, LIBMIN_UINT value){
	if(value == 0) {*dest = '0'; dest++; goto end;}
	/*Determine the highest power of 10.*/
	{ LIBMIN_UINT pow = 1;
		while(value/pow > 9) pow *= 10;
		/*found the highest power of 10*/
		while(pow){
			LIBMIN_UINT temp = value/pow; /*if we had the number 137, we would have gotten
			100 as our pow. We now divide by it to get the highest digit.*/
			*dest = (temp + ('0')); dest++;
			value -= temp * pow; /*Get rid of the highest digit.*/
			pow /= 10; /*Divide pow by 10.*/
		}
	}
	end:;
	*dest = '\0'; return;
}
#endif


#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS void mutoa_hex(char* dest, LIBMIN_UINT value);
#else
LIBMIN_FUNC_ATTRIBS void mutoa_hex(char* dest, LIBMIN_UINT value){
	if(value == 0) {*dest = '0'; dest++; goto end;}
	/*Determine the highest power of 16.*/
	{ LIBMIN_UINT pow = 1;
		while(value/pow > 0xf) pow *= 16;
		/*found the highest power of 16*/
		while(pow){
			LIBMIN_UINT temp = value/pow; /*if we had the number 137, we would have gotten
			100 as our pow. We now divide by it to get the highest digit.*/
			switch(temp){
				case 0: *dest = '0'; break;
				case 1: *dest = '1'; break;
				case 2: *dest = '2'; break;
				case 3: *dest = '3'; break;
				case 4: *dest = '4'; break;
				case 5: *dest = '5'; break;
				case 6: *dest = '6'; break;
				case 7: *dest = '7'; break;
				case 8: *dest = '8'; break;
				case 9: *dest = '9'; break;
				case 0xa: *dest = 'A'; break;
				case 0xb: *dest = 'B'; break;
				case 0xc: *dest = 'C'; break;
				case 0xd: *dest = 'D'; break;
				case 0xe: *dest = 'E'; break;
				case 0xf: *dest = 'F'; break;
			}

			dest++;
			value -= temp * pow; /*Get rid of the highest digit.*/
			pow /= 16; /*Divide pow by 10.*/
		}
	}
	end:;
	*dest = '\0'; return;
}
#endif

#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS void mitoa(char* dest, LIBMIN_INT value);
#else
LIBMIN_FUNC_ATTRIBS void mitoa(char* dest, LIBMIN_INT value){
	if(value >= 0) {mutoa(dest, (LIBMIN_UINT)value); return;}
	*dest = '-';dest++;
	value *= -1;
	mutoa(dest, value);
}
#endif


#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS void mitoa_hex(char* dest, LIBMIN_INT value, char do_prefix);
#else
LIBMIN_FUNC_ATTRIBS void mitoa_hex(char* dest, LIBMIN_INT value, char do_prefix){
	if(value >= 0) {
		if(do_prefix){
			*dest = '0';dest++;
			*dest = 'x';dest++;
		}
		mutoa_hex(dest, (LIBMIN_UINT)value); return;
	}
	*dest = '-';dest++;
	if(do_prefix){
		*dest = '0';dest++;
		*dest = 'x';dest++;
	}
	value *= -1;
	mutoa_hex(dest, value);
}
#endif


#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS LIBMIN_INT mpowi(LIBMIN_INT x, LIBMIN_INT y);
#else
LIBMIN_FUNC_ATTRIBS LIBMIN_INT mpowi(LIBMIN_INT x, LIBMIN_INT y)
{
	LIBMIN_INT r = 1;
	if(y == 0) return 1;
	if(y>1)for(;y>0;y--) r*=x;
	if(y<0)for(;y<0;y++) r/=x;
	return r;
}
#endif


#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS LIBMIN_UINT msqrti(LIBMIN_UINT x);
#else
LIBMIN_FUNC_ATTRIBS LIBMIN_UINT msqrti(LIBMIN_UINT x)
{
	LIBMIN_UINT r = 1;
	if(x==0)return 0;
	/*get within a log2*/
	while(r*r < x) r*=2;
	/*Now start decrementing.*/
	while(r*r > x) r--; 
	return r;
}
#endif


#ifndef LIBMIN_NO_FLOAT
/*
	FLOAT POINT UNIT STUFF!!!!
*/


/*Only support decimal float.*/
#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS LIBMIN_FLOAT matof(char*s);
#else
LIBMIN_FUNC_ATTRIBS LIBMIN_FLOAT matof(char*s){
	LIBMIN_FLOAT retval = 0;
	char isneg = 0;
	if(*s == '+') s++;
	else if(*s == '-') {isneg = 1; s++;}
	if(*s == 'e') goto end; /*A floating point number beginning with E has value 0 no matter how evaluated.*/

#ifndef LIBMIN_FLOAT_NO_INFINITY
	if(*s == 'I' || *s == 'i')
		if(s[1] == 'N' || s[1] == 'n')
			if(s[2] == 'F' || s[2] == 'f')
				retval = 1e100000000000;
#endif
	/*int_portion*/
	while(misdigit(*s)){
		retval *= 10;
		retval += (LIBMIN_FLOAT)((*s) - ('0'));
		s++;
	}
	if(*s == '.') goto dec_portion;
	if(*s == 'e' || *s == 'E') goto e_portion;
	goto end;
	
	dec_portion:s++;
	{
	LIBMIN_FLOAT pow;
	pow = (LIBMIN_FLOAT)1;
		while(misdigit(*s)){
			pow = pow / (LIBMIN_FLOAT)10;
			retval = retval + ( ((LIBMIN_FLOAT)((*s) - ('0'))) * pow);
			s++;
		}
		
	}
	if(*s == 'e' || *s == 'E') goto e_portion;
	goto end;
	
	e_portion:s++;
	{LIBMIN_INT sci;
	sci = matoi(s);
		if(sci == 0) goto end;
		for(;sci > 0;sci--)
			retval = retval * (LIBMIN_FLOAT)10;
		for(;sci < 0;sci++)
			retval = retval / (LIBMIN_FLOAT)10;
	}


	end:;
	if(isneg) retval *= -1;
	return retval;
}
#endif


#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS LIBMIN_FLOAT mipow(LIBMIN_FLOAT x, LIBMIN_INT y);
#else
LIBMIN_FUNC_ATTRIBS LIBMIN_FLOAT mipow(LIBMIN_FLOAT x, LIBMIN_INT y)
{
	LIBMIN_FLOAT r = 1;
	if(y == 0) return 1;
	if(y>1)for(;y>0;y--) r*=x;
	if(y<0)for(;y<0;y++) r/=x;
	return r;
}
#endif


#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS LIBMIN_FLOAT mupow(LIBMIN_FLOAT x, LIBMIN_UINT y);
#else
LIBMIN_FUNC_ATTRIBS LIBMIN_FLOAT mupow(LIBMIN_FLOAT x, LIBMIN_UINT y)
{
	LIBMIN_FLOAT r = 1;
	for(;y>0;y--) r*=x;
	return r;
}
#endif

#ifdef LIBMIN_MODE_HEADER
LIBMIN_FUNC_ATTRIBS void mftoa(char* dest, float value, LIBMIN_UINT after_decimal);
#else
LIBMIN_FUNC_ATTRIBS void mftoa(char* dest, float value, LIBMIN_UINT after_decimal){

	if(value < 0) {*dest = '-'; dest++;value = value * -1;}
	if(value == 0) {*dest = '0'; dest++; goto end;}
#ifndef LIBMIN_FLOAT_NO_INFINITY
	if(value == 1e100000000000){
		*dest = 'I'; dest++;
		*dest = 'N'; dest++;
		*dest = 'F'; dest++;
		goto end;
	}
#endif
	/*Determine the highest power of 10.*/
	{ 
		LIBMIN_INT pow10 = 0;
		{
			LIBMIN_FLOAT pow = 1;
			while((value/pow) >= 10) {pow *= 10;pow10++;}
		}
		/*found the highest power of 10*/
		while(pow10 >= 0){
			LIBMIN_INT temp = value/mupow(10,pow10); /*if we had the number 137.45, we would have gotten
			100 as our pow. We now divide by it to get the highest digit.*/
			*dest = ( (LIBMIN_INT)temp + ('0')); dest++;
			/*we now subtract off the 100 from 137.45 to get 37.45*/
			value = value - (
				(LIBMIN_FLOAT)temp * mupow(10,pow10)
			); /*Get rid of the highest digit.*/
			pow10--; /*Divide pow by 10.*/
		}
		/*Now print the insignificant portion.*/
		if(value == 0) {goto end;}
		*dest = '.';dest++;
		{while(after_decimal){
			LIBMIN_INT temp;
			value *= 10;
			temp = value;
			*dest = ( (LIBMIN_INT)temp + ('0')); dest++;
			value -= temp;
			after_decimal--;
		}}
	}
	end:;
	*dest = '\0'; return;
	
}
/*
	END OF FLOATING POINT UNIT STUFF!!!!
	1
	10
	10.1
	10.50379
	-5.1
	-0.00001

	0.0000002579 -> 2.579e-7

	250000 -> 2.5e5
*/
#endif



#endif
