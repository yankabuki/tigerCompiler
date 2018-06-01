#ifndef H_TRANSLATE_H
#define H_TRANSLATE_H

#include "util.h"
#include "temp.h"
#include "tree.h"
#include "frame.h"
#include "absyn.h"

typedef struct Tr_frame_ * Tr_frame;
typedef struct Tr_access_ * Tr_access;
typedef struct Tr_accessList_ * Tr_accessList;
typedef struct Tr_level_ * Tr_level;

struct Tr_accessList_ {Tr_access head; Tr_accessList tail;};

Tr_level Tr_outermost(void);
Tr_level Tr_newLevel(Tr_level parent, Temp_label name, U_boolList formals);
Tr_accessList Tr_formals(Tr_level level);
Tr_access Tr_allocLocal(Tr_level level, bool escape);

typedef struct Tr_exp_ * Tr_exp;
typedef struct patchList_ * patchList;
typedef struct Tr_expList_ * Tr_expList;

struct Tr_expList_ {Tr_exp head; Tr_expList tail;};

Tr_expList Tr_ExpList(Tr_exp head, Tr_expList tail);
Tr_exp Tr_simpleVar(Tr_access acc, Tr_level level);
Tr_exp Tr_subscriptVar(Tr_exp var, Tr_exp index);
Tr_exp Tr_fieldVar(Tr_exp var, int field_pos);
Tr_exp Tr_int(int intt);
Tr_exp Tr_string(string str);
Tr_exp Tr_call(Tr_level outer, Tr_level inner, Temp_label label, Tr_expList args);
Tr_exp Tr_break(Temp_label label);
Tr_exp Tr_while(Tr_exp test, Tr_exp body, Temp_label exit);
Tr_exp Tr_for(Tr_access acc,Tr_exp lo, Tr_exp hi, Tr_exp body, Temp_label exit);
Tr_exp Tr_if(Tr_exp test, Tr_exp then, Tr_exp elsee);
Tr_exp Tr_varDec(Tr_access acc, Tr_level level, Tr_exp init);
Tr_exp Tr_assign(Tr_exp lvalue, Tr_exp exp);
Tr_exp Tr_seq(Tr_expList seq);
Tr_exp Tr_Eseq(Tr_expList seq, Tr_exp eval);
Tr_exp Tr_array(Tr_exp size, Tr_exp init);
Tr_exp Tr_record(int size, Tr_expList init);
Tr_exp Tr_binop(T_binOp op, Tr_exp left, Tr_exp right);
Tr_exp Tr_relop(T_relOp op, Tr_exp left, Tr_exp right);
Tr_exp Tr_Nop(void);
Tr_exp Tr_Label(string name);
Tr_exp Tr_vardec(Tr_access acc, Tr_level level, Tr_exp init);
Tr_exp Tr_fundec(Tr_level level, Temp_label label, Tr_expList args, Tr_exp body);

void Tr_procEntryExit(Tr_level level, Tr_exp body, Tr_accessList formals);
F_fragList Tr_getResult(void);
#endif
