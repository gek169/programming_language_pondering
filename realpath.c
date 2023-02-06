#!/usr/local/bin/tcc -run -lpthread
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
char* rp = NULL;

char* filepath_to_directory(char* infilename){
	char* retval;
	unsigned long len;
	retval = realpath(infilename, NULL);
	if(!retval) return NULL;
	len = strlen(retval);
	len--;
	while(retval[len] != '/' &&
	retval[len] != '\\'){
		retval[len] = '\0';
		len--;	
	}
	return retval;
}


int main(int argc, char**argv){
	if(argc < 2) return 1;
	rp = realpath(argv[1],0);
	if(rp){
		printf("%s\n",rp);
		free(rp);
		rp = filepath_to_directory(argv[1]);
		printf("%s\n",rp);		
	}else
		puts("Could not resolve path.");

	if(rp)free(rp);
	return 0;
}
