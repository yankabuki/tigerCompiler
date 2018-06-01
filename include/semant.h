#ifndef H_SEMANT_H
#define H_SEMANT_H

#include "env.h"
#include "absyn.h"
#include "types.h"
#include "translate.h"

struct expty {Tr_exp exp; Ty_ty ty; bool read_only;};

struct expty expTy(Tr_exp exp, Ty_ty  ty);
struct expty transVar(Tr_level lev, S_table venv, S_table tenv, A_var v);
struct expty transExp(Tr_level lev, S_table venv, S_table tenv, A_exp e);
Tr_exp transDec(Tr_level lev, S_table venv, S_table tenv, A_dec d);
Ty_ty transTy(Tr_level lev, S_table tenv,  A_ty a);
F_fragList SEM_transProg(A_exp exp);
#endif
