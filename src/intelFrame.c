#include "util.h"
#include "frame.h"
#include "temp.h"
#include "types.h"

struct F_frame_ {
	F_accessList formals;
	F_accessList locals;
	int fcount;
	int inregCount;
	int inframeCount;
	Temp_label name;
};

F_frag F_StringFrag(Temp_label label, string str) {
	F_frag f = checked_malloc(sizeof(*f));
	f->kind = F_stringFrag;
	f->u.stringg.label = label;
	f->u.stringg.str = str;
	return f;
}

F_frag F_VarFrag(Temp_label label, int size) {
	F_frag f = checked_malloc(sizeof(*f));
	f->kind = F_varFrag;
	f->u.var.label = label;
	f->u.var.size = size;
	return f;
}

F_frag F_ProcFrag(T_stm body, F_frame frame) {
	F_frag f = checked_malloc(sizeof(*f));
	f->kind = F_procFrag;
	f->u.proc.body = body;
	f->u.proc.frame = frame;
	return f;
}

F_fragList F_FragList(F_frag head, F_fragList tail) {
	F_fragList l = checked_malloc(sizeof(*l));
	l->head = head;
	l->tail = tail;
	return l;
}

static F_access InFrame(int offset) {
	F_access p = checked_malloc(sizeof(*p));
	p->kind = inFrame;
	p->u.offset = offset;
	return p;
}

static F_access InReg(Temp_temp reg) {
	F_access p = checked_malloc(sizeof(*p));
	p->kind = inReg;
	p->u.reg = reg;
	return p;
}

F_accessList F_newAccessList(F_access head, F_accessList tail) {
	F_accessList l = checked_malloc(sizeof(*l));
	l->head = head;
	l->tail = tail;
	return l;
}

//TRUE escapa
//FALSE ñ escapa
F_frame F_newFrame(Temp_label name, U_boolList formals) {
	F_frame frame = checked_malloc(sizeof(*frame));
	frame->name = name;
	frame->fcount = 0;
	frame->inregCount = 0;
	frame->inframeCount = 0;
	U_boolList formal;
	F_accessList a = NULL;
	for(formal = formals; formal; formal = formal->tail) {
		F_access f;
		if(formal->head)
			f = InFrame(WORD * (1 + ++frame->fcount));
		else
			f = InReg(Temp_newtemp());
		if(frame->formals == NULL) {
			a = frame->formals = F_newAccessList(f, NULL);
		}
		else {
			a = a->tail = F_newAccessList(f, NULL);
		}
	}
	return frame;
}

Temp_label F_name(F_frame f) {
	return f->name;
}

F_accessList F_formals(F_frame f) {
	return f->formals;
}

F_access F_allocLocal(F_frame f, bool escape) {
	F_access a;
	if (escape) {
		f->inframeCount++;
		a = InFrame(WORD * -(f->inframeCount)); //FP
	}
	else {
		f->inregCount++;
		a = InReg(Temp_newtemp());
	}
	if (f->locals) {
		F_accessList l = f->locals;
		while(l->tail) l = l->tail;
		l->tail = F_newAccessList(a, NULL);
	}
	else {
		f->locals = F_newAccessList(a, NULL);
	}
	return a;
}

int F_offset(F_access access) {
	return access->u.offset;
}

int F_size(F_frame f) {
	return f->inframeCount + f->fcount;
}

int F_formalCount(F_frame f) {
	return f->fcount;
}

int F_frameCount(F_frame f) {
	return f->inframeCount;
}

void F_initRegisters() {
	F_FP = Temp_newregister("ebp");
	F_SP = Temp_newregister("esp");
	F_EAX = Temp_newregister("eax");
	F_EBX = Temp_newregister("ebx");
	F_ECX = Temp_newregister("ecx");
	F_EDX = Temp_newregister("edx");
	F_ESI = Temp_newregister("esi");
	F_EDI = Temp_newregister("edi");
}

Temp_tempList F_callerSaves(void) {
	Temp_tempList registers = NULL;
	registers = Temp_TempList(F_EAX,
				Temp_TempList(F_ECX,
				Temp_TempList(F_EDX, NULL)));
	return registers;
}

Temp_tempList F_calleeSaves(void) {
	Temp_tempList registers = NULL;
	registers = Temp_TempList(F_EBX,
				Temp_TempList(F_EDI,
				Temp_TempList(F_ESI, NULL)));
	return registers;
}

Temp_tempList F_specialRegs(void) {
	Temp_tempList registers = NULL;
	registers = Temp_TempList(F_FP,
				Temp_TempList(F_SP, NULL));
	return registers;
}

T_exp F_Exp(F_access acc, T_exp framePtr, int offset) {
	switch(acc->kind) {
		case inFrame:
			return T_Mem(T_Binop(T_plus, T_Const(acc->u.offset + offset), framePtr));
			break;
		case inReg:
			return T_Temp(acc->u.reg);
			break;
	}
}

T_exp F_externalCall(string s, T_expList args) {
	return T_Call(T_Name(Temp_namedlabel(s)), args, 0);
}

T_stm F_procEntryExit1(F_frame frame, T_stm stm) {
	return stm;
}

AS_instrList F_procEntryExit2(AS_instrList body) {
	 Temp_tempList returnSink = F_specialRegs();
	 returnSink->tail = Temp_TempList(F_EAX, NULL);
	return AS_splice(body, AS_InstrList(AS_Oper("", NULL, returnSink, NULL), NULL));
}

int offset = 0;
AS_instrList F_saveRegisters(F_frame frame, Temp_tempList regs) {
	if(regs == NULL)
		return NULL;
	else {
		char buffer[128];
		F_access f = F_allocLocal(frame, TRUE);
		offset = f->u.offset;
		sprintf(buffer, "movl\t%%`s0, %i(%%`d0) #saving\n", f->u.offset);
		return AS_InstrList(AS_Move(buffer,
							Temp_TempList(F_FP, NULL),
							Temp_TempList(regs->head, NULL)),
							F_saveRegisters(frame, regs->tail));
	}
}

AS_instrList F_loadRegisters(F_frame frame, Temp_tempList regs, int offset) {
	if(regs == NULL)
		return NULL;
	else {
		char buffer[128];
		sprintf(buffer, "movl\t%i(%%`s0), %%`d0 #restoring\n", offset);
		return AS_InstrList(AS_Move(buffer,
							Temp_TempList(regs->head, NULL),
							Temp_TempList(F_FP, NULL)),
							F_loadRegisters(frame, regs->tail, offset - WORD));
	}
}

AS_proc F_procEntryExit3(F_frame frame, AS_instrList body) {
	char buf[100];
#ifdef DEBUG
	printf("%s frame size: %i\n\n", S_name(F_name(frame)), WORD * frame->inframeCount);
#endif
	body = AS_splice(F_saveRegisters(frame, F_calleeSaves()), body);
	body = AS_splice(body, F_loadRegisters(frame, F_calleeSaves(), (2 * WORD) + offset));
	sprintf(buf, "addl\t$%i, %%`d0 \t#remove %i words from stack\nleave\nret\n",
			WORD*frame->inframeCount,
			frame->inframeCount);
	AS_instr ret = AS_Oper(buf, Temp_TempList(F_SP, NULL), Temp_TempList(F_SP, Temp_TempList(F_FP, NULL)), NULL);
	body = AS_splice(body, AS_InstrList(ret, NULL));
	sprintf(buf, "pushl\t%%`s0\t#save fp\nmovl\t%%`s1, %%`d0\nsubl\t$%i, %%`d1\n",
			WORD * frame->inframeCount);
	Temp_tempList special_regs = Temp_TempList(F_FP,Temp_TempList(F_SP, NULL));
	AS_instr init = AS_Oper(buf, special_regs, special_regs, NULL);
	body = AS_splice(AS_InstrList(init, NULL), body);
	sprintf(buf, ".globl\t%s\n"
	              ".type\t%s, @function\n"
				  "%s:\t#begin proc\n",
				  S_name(frame->name),
				  S_name(frame->name),
				  S_name(frame->name));
	return AS_Proc(String(buf), body, "#end proc\n\n");
}
