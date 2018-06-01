#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRUE 1
#define FALSE 0

//TODO Adicionar sobrecarga de matriz
char consts[256];
int main() {
	int i;
	for(i = 0; i < 256; i++) {
		consts[i] = i;
	}
	return tigermain(0 /* static link */);
}

void eexit(int code) {
	exit(code);
}
int *initArray(int size, int init) {
	int i;
	int *a = (int *)malloc(size*sizeof(int));
	for(i = 0; i < size; i++)
		a[i] = init;
	return a;
}

int *allocRecord(int size) {
	int i;
	int *p, *a;
	p = a = (int *)malloc(size);
	for(i = 0; i < size; i += sizeof(int))
		*p++ = 0;
	return a;
}

int toint(char * s) {
	return atoi(s);
}

char * tostr(int n) {
	char * r = malloc(sizeof(char) * 16);
	sprintf(r, "%i", n);
	return r;
}

void print(char * s) {
	printf("%s", s);
}

void flush() {
 fflush(stdout);
}

int ord(char * s) {
 return (int)s[0];
}

char * chr(int i) {
 if (i < 0 || i >= 256) {
	 printf("chr(%d) out of range\n", i);
	 exit(1);
 }
 char * ret = malloc(sizeof(char) * 2);
 ret[0] = consts[i];
 ret[1] = consts[0];
 return ret;
}

char * getchr() {
	int i = getc(stdin);
	if (i==EOF)
		return "";
	else {
		 return chr(i);
	}
}

int _strCompare(char * string1, char * string2) {
	if(string1 == NULL && string2 == NULL)
		return TRUE;
	if(string1 == NULL || string2 == NULL)
		return FALSE;
	return strcmp(string1, string2) == 0 ? TRUE: FALSE;
}

char * _strConcat(char * string1, char * string2) {
	char * ret = malloc(sizeof(char) * (strlen(string1) + strlen(string2)));
	strcpy(ret, string1);
	strcat(ret, string2);
	return ret;
}

char * _strRemove(char * string1, char * string2) {
	int length = strlen(string2);
	char * p = string1;
	char * ret = malloc(sizeof(char) * strlen(string1));
	char * i;
	int n = 0;
	while((i = strstr(p, string2)) != NULL) {
		int j;
		for(j = 0; j < (int)i - (int)p; j++) {
			ret[n] = p[j];
			n++;
		}
		p = strstr(p, string2) + length;
	}
	strcpy(ret + n, p);
	return ret;
}

char * _strMultiply(char * string1, int times) {
	char * ret = malloc(sizeof(char) * (times * strlen(string1)));
	strcpy(ret, "");
	int i;
	for(i = 0; i < times; i++) strcat(ret, string1);
	return ret;
}

int _arrayCompare(int * array1, int * array2) {
	if(array1[0] != array2[0]) return FALSE;
	if(array1 == NULL && array2 == NULL)
		return TRUE;
	if(array1 == NULL || array2 == NULL)
		return FALSE;
	return memcmp(array1, array2, array1[0] + 4) == 0 ? TRUE: FALSE;
}

int * _arrayAppend(int * array1, int * array2) {
	int size1 = array1[0];
	int size2 = array2[0];
	int * ret = malloc(sizeof(int) * (size1 + size2 + 1));
	memcpy(ret + 1, array1 + 1, size1 * 4);
	memcpy(ret + size1 + 1, array2 + 1, size2  * 4);
	ret[0] = size1 + size2;
	return ret;
}

int * _arrayMultiply(int * array1, int times) {
	int size = array1[0];
	int * ret = malloc(sizeof(int) * (times * (size + 1)));
	int * p = ret;
	p = p + 1;
	int i;
	for(i = 1; i <= times; i++) {
		memcpy(p, array1 + 1, size * 4);
		p = p + size;
	}
	ret[0] = times * size;
	return ret;
}
