#ifndef H_FRAME_H
#define H_FRAME_H

#include "util.h"
#include "temp.h"
#include "tree.h"
#include "assem.h"

#define K 6
#define WORD 4

typedef struct F_frame_ * F_frame;
typedef struct F_access_ * F_access;
typedef struct F_accessList_ * F_accessList;
typedef struct F_frag_ * F_frag;
typedef struct F_fragList_ * F_fragList;
Temp_map F_tempMap;

struct F_access_ {
	enum {inFrame, inReg} kind;
	union {
		int offset;
		Temp_temp reg;
	} u;
};

struct F_fragList_ {F_frag head; F_fragList tail;};

struct F_frag_ {
	enum {F_stringFrag, F_procFrag, F_varFrag} kind;
	union {
		struct {Temp_label label; string str;} stringg;
		struct {T_stm body; F_frame frame;} proc;
		struct {Temp_label label; int size;} var;
	} u;
};

F_frag F_StringFrag(Temp_label label, string str);
F_frag F_ProcFrag(T_stm body, F_frame frame);
F_fragList F_FragList(F_frag head, F_fragList tail);

struct F_accessList_ {F_access head; F_accessList tail;};

F_frame F_newFrame(Temp_label name, U_boolList formals);
Temp_label F_name(F_frame f);
F_accessList F_formals(F_frame f);
F_access F_allocLocal(F_frame f, bool escape);
int F_offset(F_access access);
int F_size(F_frame f);
int F_formalCount(F_frame f);
int F_frameCount(F_frame f);

Temp_temp F_FP;
Temp_temp F_SP;
Temp_temp F_EAX;
Temp_temp F_EBX;
Temp_temp F_ECX;
Temp_temp F_EDX;
Temp_temp F_ESI;
Temp_temp F_EDI;

void F_initRegisters();
Temp_tempList F_callerSaves(void);
Temp_tempList F_calleeSaves(void);
Temp_tempList F_specialRegs(void);

T_exp F_Exp(F_access acc, T_exp framePtr, int offset);
T_exp F_externalCall(string s, T_expList args);
T_stm F_procEntryExit1(F_frame frame, T_stm stm);
AS_instrList F_procEntryExit2(AS_instrList body);
AS_proc F_procEntryExit3(F_frame frame, AS_instrList body);
#endif
