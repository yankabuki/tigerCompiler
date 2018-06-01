#include <stdlib.h>
#include "util.h"
#include "frame.h"
#include "translate.h"
#include "tree.h"

struct Tr_access_ {
    Tr_level level;
    F_access access;
    bool isParam;
};

struct Tr_level_ {
	Tr_level parent;
	Tr_accessList locals;
	Tr_accessList formals;
	F_frame frame;
};

struct Cx {
	patchList trues;
	patchList falses;
	T_stm stm;
};

struct Tr_exp_ {
	enum {Tr_ex, Tr_nx, Tr_cx} kind;
	union {T_exp ex; T_stm nx; struct Cx cx;} u;
};


static F_fragList frags = NULL;

F_fragList Tr_fragTail(F_fragList frags) {
	if(frags->tail == NULL)
		return frags;
	else
		return Tr_fragTail(frags->tail);
}

void Tr_addFrag(F_frag f) {
	if(frags == NULL) {
		frags =  F_FragList(f, NULL);
	}
	else {
		F_fragList t = Tr_fragTail(frags);
		t->tail = F_FragList(f, NULL);
	}
}

//labels que apontam para os endereços não conhecidos, facilita resolve-los
struct patchList_ {
	Temp_label * head;
	patchList tail;
};

Tr_expList Tr_ExpList(Tr_exp head, Tr_expList tail) {
	Tr_expList l = checked_malloc(sizeof(*l));
	l->head= head;
	l->tail = tail;
	return l;
}

static Tr_access Tr_newAccess(Tr_level level, F_access access) {
    Tr_access a = checked_malloc(sizeof(*a));
    a->level = level;
    a->access = access;
    a->isParam = FALSE;
    return a;
}

Tr_accessList Tr_newAccessList(Tr_access head, Tr_accessList tail) {
	Tr_accessList a = checked_malloc(sizeof(*a));
	a->head = head;
	a->tail = tail;
	return a;
}

static Tr_level _tr_outermost = NULL;

Tr_level Tr_outermost(void) {
	if(_tr_outermost == NULL)
		_tr_outermost = Tr_newLevel(NULL, Temp_namedlabel("tigermain"), NULL);
	return _tr_outermost;
}

Tr_accessList convert_accesslist(Tr_level level, F_accessList acc) {
	if(acc == NULL)
		return NULL;
	else {
		Tr_access a = Tr_newAccess(level, acc->head);
		a->isParam = TRUE;
		return Tr_newAccessList(a, convert_accesslist(level, acc->tail));
	}
}

Tr_level Tr_newLevel(Tr_level parent, Temp_label name, U_boolList formals) {
	Tr_level l = checked_malloc(sizeof(*l));
	l->parent = parent;
	l->frame = F_newFrame(name, U_BoolList(TRUE, formals));
	l->formals = convert_accesslist(l, F_formals(l->frame));
	return l;
}

Tr_accessList Tr_formals(Tr_level level) {
	return level->formals;
}

Tr_access Tr_allocLocal(Tr_level level, bool escape) {
	F_access fr_a = F_allocLocal(level->frame, escape);
	Tr_access tr_a = Tr_newAccess(level, fr_a);

	if (level->locals) {
		Tr_accessList l = level->locals;
		while(l->tail) l = l->tail;
		l->tail = Tr_newAccessList(tr_a, NULL);
	}
	else {
		level->locals = Tr_newAccessList(tr_a, NULL);
	}
	return tr_a;
}

void doPatch(patchList tList, Temp_label label) {
	for(; tList; tList = tList->tail) {
		*(tList->head) = label;
	}
}

patchList joinPatch(patchList first, patchList second) {
	if(!first) return second;
	for(; first->tail; first = first->tail);
	first->tail = second;
	return first;
}

static Tr_exp Tr_Ex(T_exp ex) {
	Tr_exp e = checked_malloc(sizeof(*e));
	e->kind = Tr_ex;
	e->u.ex = ex;
	return e;
}

static Tr_exp Tr_Nx(T_stm nx) {
	Tr_exp e = checked_malloc(sizeof(*e));
	e->kind = Tr_nx;
	e->u.nx= nx;
	return e;
}

static Tr_exp Tr_Cx(patchList trues, patchList falses, T_stm stm) {
	Tr_exp e = checked_malloc(sizeof(*e));
	e->kind = Tr_cx;
	e->u.cx.trues = trues;
	e->u.cx.falses = falses;
	e->u.cx.stm = stm;
	return e;
}

static patchList PatchList(Temp_label * head, patchList tail) {
	patchList p = checked_malloc(sizeof(*p));
	p->head = head;
	p->tail = tail;
	return p;
}

static T_exp unEx(Tr_exp e) {
	switch(e->kind) {
		case Tr_ex:
			return e->u.ex;

		case Tr_cx: {
			Temp_temp r = Temp_newtemp();
			Temp_label t = Temp_newlabel(), f = Temp_newlabel();
			doPatch(e->u.cx.trues, t);
			doPatch(e->u.cx.falses, f);
			return T_Eseq(T_Move(T_Temp(r), T_Const(1)),
					T_Eseq(e->u.cx.stm,
					T_Eseq(T_Label(f),
					T_Eseq(T_Move(T_Temp(r), T_Const(0)),
					T_Eseq(T_Label(t), T_Temp(r))))));
		}

		case Tr_nx:
			return T_Eseq(e->u.nx, T_Const(0));
	}
}

static T_stm unNx(Tr_exp e) {
	switch(e->kind) {
		case Tr_ex:
			return T_Exp(e->u.ex);

		case Tr_nx:
			return e->u.nx;

		/*equivalente a
		 if stm {}
		 else {}
		 */
		case Tr_cx: {
			Temp_label label = Temp_newlabel();
			doPatch(e->u.cx.trues, label);
			doPatch(e->u.cx.falses, label);
			return T_Seq(e->u.cx.stm, T_Seq(T_Label(label) ,NULL));
		}
	}
}

static struct Cx unCx(Tr_exp e) {
	switch(e->kind) {
		case Tr_ex: {
			struct Cx cx;
			cx.stm = T_Cjump(T_eq, e->u.ex, T_Const(1), NULL, NULL);
			cx.trues = PatchList(&(cx.stm->u.CJUMP.true), NULL);
			cx.falses = PatchList(&(cx.stm->u.CJUMP.false), NULL);
			return cx;
		}

		case Tr_nx:
			assert(0);

		case Tr_cx:
			return e->u.cx;
	}
}

Tr_exp Tr_simpleVar(Tr_access acc, Tr_level level) {
    if(acc->level == level)
    	return Tr_Ex(F_Exp(acc->access, T_Temp(F_FP), 0));
    Tr_level i = level;
    T_exp staticLink = NULL;
    while(acc->level != i) {
    	if(staticLink == NULL)
    		staticLink = T_Mem(T_Binop(T_plus, T_Const(0), T_Temp(F_FP)));
    	else
    		staticLink = T_Mem(T_Binop(T_plus, T_Const(0), staticLink));
        i = i->parent;
    }
    return Tr_Ex(F_Exp(acc->access, staticLink, 0));
}

//NOTE: TESTED!
Tr_exp Tr_subscriptVar(Tr_exp var, Tr_exp index) {
	return Tr_Ex(T_Mem(T_Binop(T_plus, unEx(var),
							T_Binop(T_mul, T_Const(WORD),
								T_Binop(T_plus, T_Const(1), unEx(index))))));
}

//NOTE: TESTED!
Tr_exp Tr_fieldVar(Tr_exp var, int field_pos) {
	return Tr_Ex(T_Mem(T_Binop(T_plus, unEx(var), T_Const(field_pos * WORD))));
}

//NOTE: TESTED!
Tr_exp Tr_int(int intt) {
	return Tr_Ex(T_Const(intt));
}

//NOTE: TESTED!
Tr_exp Tr_string(string str) {
	Temp_label label = Temp_newlabel();
	Tr_addFrag(F_StringFrag(label, str));
	return Tr_Ex(T_Name(label));
}

static T_expList convert_explist(Tr_expList list) {
	if(list == NULL)
		return NULL;
	else
		return T_ExpList(unEx(list->head), convert_explist(list->tail));
}

//NOTE: TESTED!
Tr_exp Tr_call(Tr_level outer, Tr_level inner, Temp_label label, Tr_expList args) {
	if(outer->parent == inner) {
#ifdef DEBUG
	printf("follow static link: %i -> %s\n", 0, S_name(label));
#endif
		return Tr_Ex(T_Call(T_Name(label), convert_explist(args), 0));
	}
	Tr_level i = outer;
	int n = 0;
	while(i != inner->parent) {
	   if(i->parent == NULL) {n = 0; break;}
	   i = i->parent;
	   n++;
	}
#ifdef DEBUG
	printf("follow static link: %i -> %s\n", n, S_name(label));
#endif
	return Tr_Ex(T_Call(T_Name(label), convert_explist(args), n));
}

//NOTE: TESTED!
Tr_exp Tr_break(Temp_label label) {
	return Tr_Nx(T_Jump(T_Name(label), Temp_LabelList(label, NULL)));
}

/*
 * loop:
 * condition (true goto continue, false goto end)
 * continue:
 * body
 * goto loop
 * end:
 */
//NOTE: TESTED!
Tr_exp Tr_while(Tr_exp test, Tr_exp body, Temp_label exit) {
	Temp_label end_loop = exit;
	Temp_label loop = Temp_newlabel();
	Temp_label cont = Temp_newlabel();
	struct Cx cx = unCx(test);
	doPatch(cx.trues, cont);
	doPatch(cx.falses, end_loop);
	return Tr_Nx(T_Seq(T_Label(loop),
				  T_Seq(cx.stm,
				  T_Seq(T_Label(cont),
				  T_Seq(unNx(body),
				  T_Seq(T_Jump(T_Name(loop), Temp_LabelList(loop, NULL)),
						T_Label(end_loop)))))));
}

/*
 * loop:
 * mov var, lo //var é o contador, ele deve estar em venv para ser acessado pelas outras
 * 			   //instruções, então é preciso passar o F_access
 * var <= hi (true goto continue, false goto end)
 * cont:
 * body
 * mov var, (add var 1)
 * goto loop
 * end_loop:
 */
//NOTE: TESTED!
Tr_exp Tr_for(Tr_access acc, Tr_exp lo, Tr_exp hi, Tr_exp body, Temp_label exit) {
	Temp_label end_loop = exit;
	Temp_label loop = Temp_newlabel();
	Temp_label cont = Temp_newlabel();
	T_exp var = unEx(Tr_simpleVar(acc, acc->level));
	T_stm condition = T_Cjump(T_le, var, unEx(hi), cont, end_loop);
	return Tr_Nx(T_Seq(T_Move(var, unEx(lo)),
				  T_Seq(T_Label(loop),
				  T_Seq(condition,
				  T_Seq(T_Label(cont),
				  T_Seq(unNx(body),
				  T_Seq(T_Move(var,
						  T_Binop(T_plus, var, T_Const(1))),
				  T_Seq(T_Jump(T_Name(loop), Temp_LabelList(loop, NULL)),
				  T_Seq(T_Label(end_loop), NULL)))))))));
}

/* if-else:
 * condition (true goto cont, false goto cont_else)
 * cont_else:
 * mov ret, else-body
 * goto end:
 * cont:
 * mov ret, cont-body
 * end:
 * ret
 *
 * if:
 * condition (true goto cont, false goto end)
 * cont:
 * then-body
 * end:
 *
 *
 */
//NOTE: TESTED!
Tr_exp Tr_if(Tr_exp test, Tr_exp then, Tr_exp elsee) {
	Temp_label cont = Temp_newlabel();
	Temp_label cont_else = Temp_newlabel();
	Temp_label end = Temp_newlabel();
	struct Cx condition = unCx(test);
	if(elsee) {
		doPatch(condition.trues, cont);
		doPatch(condition.falses, cont_else);
		T_exp ret = T_Temp(Temp_newtemp());
		T_exp r = T_Eseq(T_Seq(condition.stm,
	             T_Seq(T_Label(cont_else),
	             T_Seq(T_Move(ret, unEx(elsee)),
	             T_Seq(T_Jump(T_Name(end), Temp_LabelList(end, NULL)),
	             T_Seq(T_Label(cont),
	             T_Seq(T_Move(ret, unEx(then)),
	             T_Seq(T_Jump(T_Name(end), Temp_LabelList(end, NULL)),
	            	   T_Label(end)))))))),
				 ret);
		return Tr_Ex(r);
	}
	else {
		doPatch(condition.trues, cont);
		doPatch(condition.falses, end);
		T_stm r = T_Seq(condition.stm,
	             T_Seq(T_Label(cont),
	             T_Seq(unNx(then), T_Label(end))));
		return Tr_Nx(r);
	}
}

//NOTE: TESTED!
Tr_exp Tr_varDec(Tr_access acc, Tr_level level, Tr_exp init) {
    Tr_exp var = Tr_simpleVar(acc, level);
    if (init)
        return Tr_assign(var, init);
    else
        return Tr_Ex(T_Const(0));
}

//NOTE: TESTED!
Tr_exp Tr_assign(Tr_exp lvalue, Tr_exp exp) {
	return Tr_Nx(T_Move(unEx(lvalue), unEx(exp)));
}

T_stm stm_seq(Tr_expList seq) {
	if(seq->tail == NULL)
		return unNx(seq->head);
	else
		return T_Seq(unNx(seq->head), stm_seq(seq->tail));
}

//NOTE: TESTED!
Tr_exp Tr_seq(Tr_expList seq) {
	return Tr_Nx(stm_seq(seq));
}

//NOTE: TESTED!
Tr_exp Tr_Eseq(Tr_expList seq, Tr_exp eval) {
	if(seq == NULL)
		return eval;
	else
		return Tr_Ex(T_Eseq(stm_seq(seq), unEx(eval)));
}

//NOTE: TESTED!
Tr_exp Tr_array(Tr_exp size, Tr_exp init) {
	return Tr_Ex(F_externalCall("initArray",
					T_ExpList(unEx(size), T_ExpList(unEx(init), NULL))));
}

//NOTE: TESTED!
Tr_exp Tr_record(int size, Tr_expList init) {
	Temp_temp record_addr = Temp_newtemp();
	T_stm alloc = T_Move(T_Temp(record_addr),
					F_externalCall("allocRecord", T_ExpList(T_Const(size), NULL)));
	Tr_expList i;
	T_stm assigns = NULL, tail;
	int pos = 0;
	for(i = init; i; i = i->tail) {
		Temp_temp t = Temp_newtemp();
		if(assigns == NULL)
			assigns = tail = T_Seq(T_Seq(
								T_Move(T_Temp(t), unEx(i->head)),
								T_Move(T_Mem(
										T_Binop(T_plus,
											T_Const(WORD * pos),
											T_Temp(record_addr))
										), T_Temp(t))), NULL);
		else
			tail = tail->u.SEQ.right = T_Seq(T_Seq(
										T_Move(T_Temp(t), unEx(i->head)),
										T_Move(T_Mem(
												T_Binop(T_plus,
													T_Const(WORD * pos),
													T_Temp(record_addr))
												), T_Temp(t))), NULL);
		pos++;
	}
	return Tr_Ex(T_Eseq(T_Seq(alloc, assigns), T_Temp(record_addr)));
}

//NOTE: TESTED!
Tr_exp Tr_binop(T_binOp op, Tr_exp left, Tr_exp right) {
	return Tr_Ex(T_Binop(op, unEx(left), unEx(right)));
}

//NOTE: TESTED!
Tr_exp Tr_relop(T_relOp op, Tr_exp left, Tr_exp right) {
	Temp_label t = Temp_newlabel();
	Temp_label f = Temp_newlabel();
	T_stm exp = T_Cjump(op, unEx(left), unEx(right), t, f);
	return Tr_Cx(PatchList(&exp->u.CJUMP.true, NULL), PatchList(&exp->u.CJUMP.false, NULL), exp);
}

//NOTE: TESTED!
Tr_exp Tr_Label(string name) {
	return Tr_Nx(T_Label(Temp_namedlabel(name)));
}

//NOTE: TESTED!
Tr_exp Tr_Nop(void) {
	return Tr_Ex(T_Const(0));
}

void Tr_procEntryExit(Tr_level level, Tr_exp body, Tr_accessList formals) {
	F_frag proc = F_ProcFrag(F_procEntryExit1(level->frame, unNx(body)), level->frame);
	Tr_addFrag(proc);
}

F_fragList Tr_getResult(void) {
	return frags;
}
