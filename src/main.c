#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "absyn.h"
#include "canon.h"
#include "semant.h"
#include "errormsg.h"
#include "prabsyn.h"
#include "temp.h"
#include "translate.h"
#include "printtree.h"
#include "codegen.h"
#include "regalloc.h"

extern int yyparse(void);
extern A_exp absyn_root;

int argcount;
char ** arglist;
/* parse source file fname; 
   return abstract syntax data structure */
A_exp parse(string fname) {
	EM_reset(fname);
	if (yyparse() == 0) /* parsing worked */
		return absyn_root;
	else return NULL;
}

int hasOption(char * option) {
	int i;
	if(argcount == 2 && strcmp(arglist[1], "-h") == 0)
		return TRUE;
	for(i = 1; i < argcount - 1; i++) {
		if(strcmp(option, arglist[i]) == 0)
			return i;
	}
	return 0;
}

void print_temp(void * value) {
	Temp_temp temp = (Temp_temp) value;
	string s = Temp_look(Temp_name(), temp);
	printf("%s", Temp_look(Temp_name(), temp));
}

static void doProc(FILE *out, F_frame frame, T_stm body) {
	T_stmList stmList;
	AS_instrList iList;
	AS_proc proc;
	stmList = C_linearize(body);
	stmList = C_traceSchedule(C_basicBlocks(stmList));
	if(hasOption("-di"))
		printStmList(stdout, stmList);
	iList  = F_codegen(frame, stmList);
	RA_initUses();
	iList = F_procEntryExit2(iList);
	RA_regAlloc(frame, iList);
	if(hasOption("-ds"))
		AS_printInstrList(stdout, iList, Temp_name());
	RA_Alloc(iList);
	proc = F_procEntryExit3(frame, iList);
	if(hasOption("-s")) {
		fprintf(stdout, "%s", proc->prolog);
		AS_printInstrList(stdout, proc->body, Temp_name());
		fprintf(stdout, "%s", proc->epilog);
	}
	fprintf(out, "%s", proc->prolog);
	AS_printInstrList(out, proc->body, Temp_name());
	fprintf(out, "%s", proc->epilog);
}

int main(int argc, char **argv) {
	argcount = argc;
	arglist = argv;
	F_initRegisters();
	int ubound = argc;
	if(argc == 1) {
		fprintf(stderr,"usage: tc [-­d[ais]] [-­s] [-­h] [-­o <nome>] <prog.tig>\n");
		fflush(stderr);
		exit(0);
	}
	if(hasOption("-h")) {
		fprintf(stderr, "-da emitir árvore sintática abstrata\n"
						"-di emitir código intermediário\n"
						"-ds emitir código após seleção de instruções\n"
						"-s  emitir código assembly\n"
						"-o  nome do arquivo de saída\n"
						"-h  ajuda\n");
		fflush(stderr);
		exit(0);
	}
	A_exp prog = parse(argv[argc - 1]);
	if(prog == NULL)
		exit(0);
	FILE *f = fopen("proc.S", "w");
	if(hasOption("-da"))
		pr_exp(stdout, prog, 3);
	F_fragList frags = SEM_transProg(prog);
	if(EM_errorCount())
		exit(1);
	for (;frags;frags=frags->tail)
		if (frags->head->kind == F_procFrag)
			doProc(f, frags->head->u.proc.frame, frags->head->u.proc.body);
		else if (frags->head->kind == F_stringFrag)
			fprintf(f, "%s: .string\t%s\n",
					S_name(frags->head->u.stringg.label),
					frags->head->u.stringg.str);
	fclose(f);
	char build[256];
	char * filename;
	if(hasOption("-o") && hasOption("-o") + 1 != argc - 1)
		filename = argv[hasOption("-o") + 1];
	else
		filename = "a.out";
	sprintf(build, "gcc -m32 -o %s proc.S runtime/runtime.c; rm proc.S", filename);
	system(build);
	printf("\ndone!\n");
	fflush(stdout);
	return 0;
}
