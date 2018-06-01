#include <string.h>
#include "table.h"
#include "assem.h"
#include "frame.h"
#include "codegen.h"

static string op_table[4] = {"addl", "subl", "mull", "divl"};
static string jump_table[6] = {"je", "jne", "jl", "jg", "jle", "jge"};

static AS_instrList iList = NULL, last = NULL;
Temp_tempList L(Temp_temp h, Temp_tempList t) {return Temp_TempList(h,t); }

static TAB_table mMap;
bool F_memAddress(Temp_temp t) {
	return TAB_look(mMap, t) != NULL;
}

void F_enterMem(Temp_temp t) {
	TAB_enter(mMap, t, "");
}

static void emit(AS_instr inst) {
	if(last)
		last = last->tail = AS_InstrList(inst, NULL);
	else
		last = iList = AS_InstrList(inst, NULL);
}

static Temp_temp munchExp(F_frame f, T_exp e);
static void munchStm(F_frame f, T_stm s);

Temp_tempList munchArgs(F_frame f, int offset, T_expList args) {
	char buf[128];
	if(args == NULL)
		return NULL;
	Temp_temp s = munchExp(f, args->head);
	Temp_tempList r = L(s, munchArgs(f, offset + 1, args->tail));
	if(F_memAddress(s))
		sprintf(buf, "pushl\t(%%`s0) \t# arg %i\n", offset);
	else
		sprintf(buf, "pushl\t%%`s0 \t# arg %i\n", offset);
	emit(AS_Oper(buf,
			NULL,
			L(s, NULL),
			NULL));
	return r;
}

static void munchMove(F_frame f, T_exp src, T_exp dst) {
	Temp_temp d = munchExp(f, dst);
	if(src->kind == T_CONST) {
			char buffer[128];
			if(F_memAddress(d)) {
				sprintf(buffer, "movl\t$%i, (%%`s0) \t# const to mem\n", src->u.CONST);
				emit(AS_Oper(buffer,
						NULL,
						L(d, NULL),
						NULL));
			}
			else {
				sprintf(buffer, "movl\t$%i, %%`d0 \t# const to temp\n", src->u.CONST);
				emit(AS_Oper(buffer,
						L(d, NULL),
						NULL,
						NULL));
			}
			return;
	}
	Temp_temp s = munchExp(f, src);
	if(F_memAddress(d) && F_memAddress(s)) {
		Temp_temp t = Temp_newtemp();
		emit(AS_Move("movl\t(%`s0), %`d0\n",
				L(t, NULL),
				L(s, NULL)));
		emit(AS_Oper("movl\t%`s0, (%`s1) \t# move memory to memory\n",
				NULL,
				L(t, L(d, NULL)), NULL));
	}
	else if(F_memAddress(s)) {
		emit(AS_Move("movl\t(%`s0), %`d0 \t# move to memory\n",
				L(d, NULL),
				L(s, NULL)));
	}
	else if(F_memAddress(d)) {
		emit(AS_Oper("movl\t%`s0, (%`s1) \t# move to memory\n",
				NULL,
				L(s, L(d, NULL)), NULL));
	}
	else {
		emit(AS_Move("movl\t%`s0, %`d0\n",
				L(d, NULL),
				L(s, NULL)));
	}
}

static void munchStm(F_frame f, T_stm s) {
	char buffer[128];
	switch(s->kind) {
		/* SEQ não existe, foi removido na canonização
		 *
		 */
		case T_SEQ: {
			assert(0);
			return;
		}
		/*  LABEL(i)
		 *
		 *  i: onde i é uma string
		 */
		case T_LABEL: {
			sprintf(buffer, "%s:\n", Temp_labelstring(s->u.LABEL));
			emit(AS_Label(buffer, s->u.LABEL));
			return;
		}
		/*  JUMP(label, refs)
		 *
		 *  jmp label
		 */
		case T_JUMP: {
			sprintf(buffer, "jmp\t%s\n", S_name(s->u.JUMP.jumps->head));
			emit(AS_Oper(buffer,
					NULL,
					NULL,
					AS_Targets(s->u.JUMP.jumps)));
			return;
		}

		/*  CJUMP(op, left, right, trues, falses)
		 *
		 *  cmpl %left, %right
		 *  op label_true
		 *  jmp label_false
		 */
		case T_CJUMP: {
			Temp_temp left = munchExp(f, s->u.CJUMP.left);
			Temp_temp right = munchExp(f, s->u.CJUMP.right);
			if(F_memAddress(left) && F_memAddress(right)) {
				Temp_temp t = Temp_newtemp();
				emit(AS_Oper("movl\t(%`s0), %`d0\n",
						L(t, NULL),
						L(left, NULL),
						NULL));
				emit(AS_Oper("cmpl\t(%`s1), %`s0\n",
						NULL,
						L(t, L(right, NULL)),
						NULL));
			}
			else if(F_memAddress(left)) {
				emit(AS_Oper("cmpl\t%`s1, (%`s0)\n",
						NULL,
						L(left, L(right, NULL)),
						NULL));
			}
			else if(F_memAddress(right)) {
				emit(AS_Oper("cmpl\t(%`s1), %`s0\n",
						NULL,
						L(left, L(right, NULL)),
						NULL));
			}
			else {
				emit(AS_Oper("cmpl\t%`s1, %`s0\n",
						NULL,
						L(left, L(right, NULL)),
						NULL));
			}
			sprintf(buffer, "%s\t%s\n", jump_table[s->u.CJUMP.op], S_name(s->u.CJUMP.true));
			AS_instr r = AS_Oper(buffer,
							NULL,
							NULL,
							AS_Targets(Temp_LabelList(s->u.CJUMP.true, NULL)));
			r->u.OPER.cjump = TRUE;
			emit(r);
			return;
		}
		/* TODO: Melhorar um pouco o munch aqui!
		 *
		 */
		case T_MOVE: {
			T_exp src = s->u.MOVE.src;
			T_exp dst = s->u.MOVE.dst;
			if((dst->kind == T_MEM) && (dst->u.MEM->kind == T_BINOP)) {
				if((dst->u.MEM->u.BINOP.left->kind == T_CONST) &&
					(dst->u.MEM->u.BINOP.right->kind == T_TEMP)) {
						T_exp binop = dst->u.MEM;
						int offset = binop->u.BINOP.left->u.CONST;
						if(src->kind == T_CONST) {
							sprintf(buffer, "movl\t$%i, %i(%%`s0) \t# move to memory\n",
									src->u.CONST,
									offset);
							emit(AS_Oper(buffer,
									NULL,
									L(dst->u.MEM->u.BINOP.right->u.TEMP, NULL), NULL));
						}
						else {
							sprintf(buffer, "movl\t%%`s0, %i(%%`s1) \t# move to memory\n", offset);
							emit(AS_Oper(buffer,
									NULL,
									L(munchExp(f, src), L(dst->u.MEM->u.BINOP.right->u.TEMP, NULL)), NULL));
						}
				}
				else {
					munchMove(f, src, dst);
				}
			}
			else {
				munchMove(f, src, dst);
			}
			return;
		}
		/* consome o exp e ignora o retorno
		 * se exp for um mem ou um temp, salva ele no retorno
		 */
		case T_EXP: {
			if((s->u.EXP->kind == T_MEM) && (s->u.EXP->u.MEM->kind == T_BINOP)) {
				int offset = s->u.EXP->u.MEM->u.BINOP.left->u.CONST;
				Temp_temp temp = s->u.EXP->u.MEM->u.BINOP.right->u.TEMP;
				sprintf(buffer, "movl\t%i(%%`s0), %%`d0 \t# move to return\n", offset);
				emit(AS_Oper(buffer,
						L(F_EAX, NULL),
						L(temp, NULL), NULL));
			}
			else if(s->u.EXP->kind == T_TEMP) {
				emit(AS_Move("movl\t%`s0, %`d0 \t# move to return\n",
						L(F_EAX, NULL),
						L(s->u.EXP->u.TEMP, NULL)));
			}
			else {
				munchExp(f, s->u.EXP);
			}
			return;
		}
	}
}

//add sub
static Temp_temp munchOp(F_frame f, T_exp e) {
	char * l;
	char buffer[128];
	Temp_temp left = munchExp(f, e->u.BINOP.left);
	Temp_temp right = munchExp(f, e->u.BINOP.right);
	Temp_temp container = Temp_newtemp();
	if(F_memAddress(left))
		l = "movl\t(%`s0), %`d0 \t# prepare\n";
	else
		l = "movl\t%`s0, %`d0 \t# prepare\n";
	if(e->u.BINOP.op == 2 || e->u.BINOP.op == 3) {
		emit(AS_Move(l,
				L(F_EAX, NULL),
				L(left, NULL)));
		if(F_memAddress(right))
			sprintf(buffer, "%s\t(%%`s0) \t# operation\n", op_table[e->u.BINOP.op]);
		else
			sprintf(buffer, "%s\t%%`s0 \t# operation\n", op_table[e->u.BINOP.op]);
		emit(AS_Oper("movl $0, %`d0\n", L(F_EDX, NULL), NULL, NULL));
		emit(AS_Oper(buffer,
				L(F_EAX, L(F_EDX, NULL)),
				L(right, L(F_EAX, NULL)), NULL));
		return F_EAX;
	}
	else {
		emit(AS_Move(l,
				L(container, NULL),
				L(left, NULL)));
		if(F_memAddress(right))
			sprintf(buffer, "%s\t(%%`s0), %%`d0 \t# operation\n", op_table[e->u.BINOP.op]);
		else
			sprintf(buffer, "%s\t%%`s0, %%`d0 \t# operation\n", op_table[e->u.BINOP.op]);
		emit(AS_Oper(buffer,
				L(container, NULL),
				L(right, L(container, NULL)), NULL));
		return container;
	}
}

static int Args(T_expList e) {
	T_expList i;
	int n = 0;
	for(i = e; i; i = i->tail) n++;
	return n;
}

static Temp_temp munchExp(F_frame f, T_exp e) {
	char buffer[128];

	Temp_temp container = Temp_newtemp();
	switch(e->kind) {
		/*  MEM(BINOP(+, Const, EXP))
		 *  MEM(BINOP(+, EXP, Const))
		 *  MEM(EXP)
		 *  ...
		 *  resolve o interior para utilizar o exterior
		 *  é preciso salvar os temporários em uma tabela para saber se é um addr ou um valor
		 *  em local, fica fácil controlar e deixa simples a RI
		 */
		case T_MEM: {
			T_exp mem = e->u.MEM;
			F_enterMem(container);
			Temp_temp d = munchExp(f, mem);
			emit(AS_Move("movl\t%`s0, %`d0 \t# default exp\n",
					L(container, NULL),
					L(d, NULL)));
			return container;
		}

		/*  BINOP(op, CONST, EXP)
		 *  BINOP(op, EXP, CONST)
		 *  BINOP(op, EXP, EXP)
		 *
		 *  movl $1, %eax
		 *  addl %ebx, %eax onde ebx é o resultado da outra expressao
		 */
		case T_BINOP: {
			return munchOp(f, e);
		}
		/*  TEMP(i)
		 *  só retorna ele
		 */
		case T_TEMP: {
            return e->u.TEMP;
		}
		/* munchStm(ESEQ->stm)
		 * movl %ebx, %eax onde %ebx é ESEQ->exp
		 */
		case T_ESEQ: {
			munchStm(f, e->u.ESEQ.stm);
			emit(AS_Move("movl\t%`s0, %`d0 \t# evaluate\n",
					L(container, NULL),
					L(munchExp(f, e->u.ESEQ.exp), NULL)));
			return container;
		}
		/*  NAME só aparce em um EXP se for uma string
		 *  movl $string, %eax
		 */
		case T_NAME: {
			sprintf(buffer, "movl\t$%s, %%`d0 \t# string\n", S_name(e->u.NAME));
			emit(AS_Oper(buffer,
					L(container, NULL),
					NULL, NULL));
			return container;
		}
		/* CONST(i)
		 *
		 * movl $3, %eax
		 */
		case T_CONST: {
			sprintf(buffer, "movl\t$%i, %%`d0 \t# const\n", e->u.CONST);
			emit(AS_Oper(buffer,
					L(container, NULL),
					NULL, NULL));
			return container;
		}

		case T_CALL: {
			assert(e->u.CALL.fun->kind == T_NAME);
			Temp_tempList l = munchArgs(f, 0, e->u.CALL.args);
			Temp_temp t = Temp_newtemp();
			if(e->u.CALL.fetches) {
				emit(AS_Oper("movl %`s0, %`d0 \t# save fp\n",
						L(t, NULL),
						L(F_FP, NULL), NULL));
				int n = e->u.CALL.fetches;
				while(n > 0) {
					emit(AS_Oper("movl 0(%`s0), %`s1 \t# change fp\n",
							NULL,
							L(F_FP, L(F_FP, NULL)), NULL));
					n--;
				}
			}
			sprintf(buffer, "call\t%s\n", S_name(e->u.CALL.fun->u.NAME));
			emit(AS_Oper(buffer,
					F_callerSaves(),
					l,
					NULL));
			sprintf(buffer, "addl\t$%i, %%`d0\n", Args(e->u.CALL.args) * WORD);
			emit(AS_Oper(buffer,
					L(F_SP, NULL),
					NULL,
					NULL));
			if(e->u.CALL.fetches) {
				emit(AS_Oper("movl %`s0, %`d0 \t# restore fp\n",
						L(F_FP, NULL),
						L(t, NULL), NULL));
			}
			return F_EAX;
		}
	}
	assert(0);
}

AS_instrList F_codegen(F_frame f, T_stmList stmList) {
	mMap = TAB_empty();
	AS_instrList list; T_stmList sl;
	for (sl = stmList; sl; sl = sl->tail) munchStm(f, sl->head);
	list = iList; iList = last = NULL; return list;
}
