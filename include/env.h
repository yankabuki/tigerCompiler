#ifndef H_ENV_H
#define H_ENV_H

#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "types.h"
#include "translate.h"

typedef struct E_enventry_ *E_enventry;

struct E_enventry_ { 
	enum{E_varEntry, E_funEntry} kind;
	union { struct {Tr_access access; Ty_ty ty; bool read_only;} var;
		struct {Tr_level level; Temp_label label; Ty_tyList formals; Ty_ty result;} fun;
	} u;
};

E_enventry E_VarEntry(Tr_access access, Ty_ty ty, bool readonly);
E_enventry E_FunEntry(Tr_level level, Temp_label label, Ty_tyList formals, Ty_ty result);
S_table E_base_tenv(void); /*Ty_ty enviroment*/
S_table E_base_venv(void); /* E_enventry enviroment*/
#endif
            

