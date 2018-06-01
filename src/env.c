#include "env.h"

E_enventry E_VarEntry(Tr_access access, Ty_ty ty, bool readonly) {
	E_enventry ev = checked_malloc(sizeof(*ev));
	ev->u.var.ty = ty;
	ev->u.var.access = access;
	ev->kind = E_varEntry;
	ev->u.var.read_only = readonly;
	return ev;
}

E_enventry E_FunEntry(Tr_level level, Temp_label label, Ty_tyList formals, Ty_ty result) {
	E_enventry ev = checked_malloc(sizeof(*ev));
	ev->u.fun.formals = formals;
	ev->u.fun.result = result;
	ev->u.fun.level = level;
	ev->u.fun.label = label;
	ev->kind = E_funEntry;
	return ev;
}

//gera tabela de simbolos dos tipos base (de ambiente)
//dae nao da erro se usa uma tipo nao declarado tipo int
S_table E_base_tenv(void) {
	S_table tab = S_empty();
	S_enter(tab, S_Symbol("int"), Ty_Int());
	S_enter(tab, S_Symbol("string"), Ty_String());
	return tab;
}

//gera tabela de simbolos das funcoes base (de ambiente)
//dae nao da erro se usa uma funcao nao declarada tipo print
S_table E_base_venv(void) {
	S_table tab = S_empty();

	Ty_tyList params_print = Ty_TyList(Ty_String(), NULL);
	S_enter(tab, S_Symbol("print"), E_FunEntry(Tr_outermost(),
			S_Symbol("print"), params_print, Ty_Void()));

	Ty_tyList params_exit = Ty_TyList(Ty_Int(), NULL);
	S_enter(tab, S_Symbol("exit"), E_FunEntry(Tr_outermost(),
			S_Symbol("eexit"), params_exit, Ty_Void()));

	Ty_tyList params_ord = Ty_TyList(Ty_String(), NULL);
	S_enter(tab, S_Symbol("ord"), E_FunEntry(Tr_outermost(),
			S_Symbol("ord"), params_ord, Ty_Int()));

	Ty_tyList params_chr = Ty_TyList(Ty_Int(), NULL);
	S_enter(tab, S_Symbol("chr"), E_FunEntry(Tr_outermost(),
			S_Symbol("chr"), params_chr, Ty_String()));

	Ty_tyList params_tostr = Ty_TyList(Ty_Int(), NULL);
	S_enter(tab, S_Symbol("tostr"), E_FunEntry(Tr_outermost(),
			S_Symbol("tostr"), params_tostr, Ty_String()));

	Ty_tyList params_toint = Ty_TyList(Ty_String(), NULL);
	S_enter(tab, S_Symbol("toint"), E_FunEntry(Tr_outermost(),
			S_Symbol("toint"), params_toint, Ty_Int()));

	S_enter(tab, S_Symbol("getchar"), E_FunEntry(Tr_outermost(),
			S_Symbol("getchr"), NULL, Ty_String()));
	return tab;
}
