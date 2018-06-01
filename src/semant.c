#include <stdio.h>
#include <stdlib.h>
#include "semant.h"
#include "translate.h"
#include "errormsg.h"

static Temp_label breakk;

static char str_ty[][12] = {
   "record", "nil", "int", "string",
   "array", "name", "void"};

Ty_ty actual_ty(Ty_ty ty) {
	while((ty != NULL) && (ty->kind == Ty_name)) {
		ty = ty->u.name.ty;
		if(!ty)
			return NULL;
	}
	return ty;
}

struct expty expTy(Tr_exp exp, Ty_ty  ty) {
	if(EM_errorCount())
		exit(1);
	struct expty e;
	e.exp = exp;
	e.ty = actual_ty(ty);
	e.read_only = 0;
	return e;
}

Tr_exp overloadCall(string function, Tr_exp left, Tr_exp right) {
	return Tr_call(Tr_outermost(), Tr_outermost(), Temp_namedlabel(function),
					Tr_ExpList(left, Tr_ExpList(right, NULL)));
}

struct expty transVar(Tr_level lev, S_table venv, S_table tenv, A_var v) {
	switch(v->kind) {
		case A_simpleVar: {
			E_enventry x = S_look(venv, v->u.simple);
			if(x && x->kind == E_varEntry) {
				struct expty exp = expTy(Tr_simpleVar(x->u.var.access, lev), actual_ty(x->u.var.ty));
				exp.read_only = x->u.var.read_only;
				return exp;
			}
			else {
				EM_error(v->pos, "undefined variable \"%s\"", S_name(v->u.simple));
				return expTy(NULL, Ty_Int());
			}
		}

        case A_subscriptVar: {
			struct expty array = transVar(lev, venv, tenv, v->u.subscript.var);
            if(actual_ty(array.ty)->kind != Ty_array) {
                EM_error(v->pos, "not an array type");
                return expTy(NULL,Ty_Int());
            }
			struct expty index = transExp(lev, venv, tenv, v->u.subscript.exp);
			if(index.ty->kind != Ty_int) {
				EM_error(v->pos, "index must be int");
				return expTy(NULL, Ty_Int());
			}

			return expTy(Tr_subscriptVar(array.exp, index.exp), actual_ty(array.ty->u.array));
        }

        case A_fieldVar: {
            struct expty rec = transVar(lev, venv, tenv, v->u.field.var);
            if(actual_ty(rec.ty)->kind != Ty_record) {
                EM_error(v->pos, "type not a record");
                return expTy(NULL, Ty_Int());
            }
            Ty_fieldList fields;
            int pos = 0;
            for(fields = rec.ty->u.record; fields; fields = fields->tail) {
                if(fields->head->name == v->u.field.sym)
                    return expTy(Tr_fieldVar(rec.exp, pos), fields->head->ty);
                pos++;
            }
            EM_error(v->pos,"no field named \"%s\"", S_name(v->u.field.sym));
            return expTy(NULL, Ty_Int());
		}
	}
}

Ty_ty ty_last;
Tr_expList transList(Tr_level lev, S_table venv, S_table tenv, A_expList exps) {
	if(exps == NULL)
		return NULL;
	else {
		struct expty exp = transExp(lev, venv, tenv, exps->head);
		ty_last = exp.ty;
		return Tr_ExpList(exp.exp, transList(lev, venv, tenv, exps->tail));
	}
}

Tr_expList transDecList(Tr_level lev, S_table venv, S_table tenv, A_decList decs) {
	if(decs == NULL)
		return NULL;
	else {
		Tr_exp exp = transDec(lev, venv, tenv, decs->head);
		return Tr_ExpList(exp, transDecList(lev, venv, tenv, decs->tail));
	}
}

struct expty transExp(Tr_level lev, S_table venv, S_table tenv, A_exp e) {
	if(e == NULL) return expTy(Tr_Nop(), Ty_Void());
	switch(e->kind) {
		case A_varExp: {
			return transVar(lev, venv, tenv, e->u.var);
		}

		case A_nilExp: {
			return expTy(Tr_int(0), Ty_Nil());
		}

		case A_intExp: {
			return expTy(Tr_int(e->u.intt), Ty_Int());
		}

		case A_stringExp: {
			return expTy(Tr_string(e->u.stringg), Ty_String());
		}

		case A_callExp: {
			E_enventry call = S_look(venv, e->u.call.func);
			Ty_tyList param;
			A_expList call_param = e->u.call.args;
			Tr_expList args = NULL, tail = NULL;
			//not declared
			if(!call) {
				EM_error(e->pos, "in function \"%s\": undeclared", S_name(e->u.call.func));
				return expTy(NULL, Ty_Nil());
			}
			else {
				for(param = call->u.fun.formals; param; param = param->tail) {
					if(!call_param) {
						EM_error(e->u.call.args->head->pos,
								"in function \"%s\": not enough parameters",
								S_name(e->u.call.func));
						return expTy(NULL, actual_ty(call->u.fun.result));
					}
					struct expty exp = transExp(lev, venv, tenv, call_param->head);
					if(args == NULL) {
						args = tail = Tr_ExpList(exp.exp, NULL);
					}
					else {
						tail = tail->tail = Tr_ExpList(exp.exp, NULL);
					}
					exp.ty = actual_ty(exp.ty);
					param->head = actual_ty(param->head);
					if(exp.ty != param->head) {
						EM_error(call_param->head->pos,
								"in function \"%s\": param type mismatch, have \"%s\" and \"%s\"", S_name(e->u.call.func),
								str_ty[param->head->kind],
								str_ty[exp.ty->kind]);
					}
					call_param = call_param->tail;
				}
				if(call_param) {
					EM_error(e->u.call.args->head->pos,"in function \"%s\": too much parameters",
							S_name(e->u.call.func));
					return expTy(NULL, actual_ty(call->u.fun.result));
				}
			}
			return expTy(Tr_call(call->u.fun.level, lev, call->u.fun.label, args), actual_ty(call->u.fun.result));
		}

        case A_breakExp: {
        	E_enventry loop = S_look(venv, S_Symbol("1_dentro_de_laço"));
        	if(!loop) {
        		EM_error(e->pos, "break outside loop expression");
        	}
        	return expTy(Tr_break(breakk), Ty_Void());
        }

		case A_whileExp: {
			Temp_label old_breakk = breakk;
			breakk = Temp_newlabel();
			struct expty test = transExp(lev, venv, tenv, e->u.whilee.test);
			S_beginScope(venv); //cheat
			S_enter(venv, S_Symbol("1_dentro_de_laço"), E_VarEntry(NULL, Ty_Int(), TRUE));
			struct expty body = transExp(lev, venv, tenv, e->u.whilee.body);
			S_endScope(venv);
			if(test.ty->kind != Ty_int) {
				EM_error(e->pos, "for condition must be int");
				return expTy(NULL, Ty_Nil());
			}
			if(body.ty->kind != Ty_void) {
            	EM_error(e->pos, "for body must return void");
            	return expTy(NULL, Ty_Void());
            }
            struct expty ret = expTy(Tr_while(test.exp, body.exp, breakk), Ty_Void());
			breakk = old_breakk;
			return ret;
		}

		case A_forExp: {
			Temp_label old_breakk = breakk;
			breakk = Temp_newlabel();
			Tr_access var = Tr_allocLocal(lev, e->u.forr.escape);
			struct expty lo = transExp(lev, venv, tenv, e->u.forr.lo);
			struct expty hi = transExp(lev, venv, tenv, e->u.forr.hi);
			if(lo.ty->kind != Ty_int) {
				EM_error(e->pos, "for lower bound must be int");
			}
			if(hi.ty->kind != Ty_int) {
				EM_error(e->pos, "for upper bound must be int");
			}

            S_beginScope(venv);
            S_beginScope(tenv);
            S_enter(venv, e->u.forr.var, E_VarEntry(var, Ty_Int(), FALSE));
            S_enter(venv, S_Symbol("1_dentro_de_laço"), E_VarEntry(NULL, Ty_Int(), TRUE));//cheat
			struct expty body = transExp(lev, venv, tenv, e->u.forr.body);
            S_endScope(tenv);
            S_endScope(venv);

            if(body.ty->kind != Ty_void) {
            	EM_error(e->pos, "for body must return void");
            	return expTy(NULL, Ty_Void());
            }
            struct expty ret = expTy(Tr_for(var, lo.exp, hi.exp, body.exp, breakk), Ty_Void());
			breakk = old_breakk;
			return ret;
		}

		case A_ifExp: {
			struct expty test = transExp(lev, venv, tenv, e->u.iff.test);
			struct expty then = transExp(lev, venv, tenv, e->u.iff.then);
			struct expty elsee = transExp(lev, venv, tenv, e->u.iff.elsee);
			if(e->u.iff.elsee == NULL) {
				elsee.exp = NULL;
			}
			if(test.ty->kind != Ty_int) {
				EM_error(e->pos, "if condition must be int");
				return expTy(NULL, Ty_Int());
			}
			if(e->u.iff.elsee) {
                if((then.ty->kind == Ty_record) && (elsee.ty->kind == Ty_nil))
                    return expTy(Tr_if(test.exp, then.exp, elsee.exp), actual_ty(then.ty));
                if((then.ty->kind == Ty_nil) && (elsee.ty->kind == Ty_record))
                    return expTy(Tr_if(test.exp, then.exp, elsee.exp), actual_ty(elsee.ty));
				if(elsee.ty != then.ty) {
					EM_error(e->pos, "if expression type mismatch, have \"%s\" and \"%s\"",
							str_ty[then.ty->kind],
							str_ty[elsee.ty->kind]);
					return expTy(NULL, actual_ty(then.ty));
				}
			}
			else if(then.ty->kind != Ty_void) {
				EM_error(e->pos, "if-then must return void");
				return expTy(NULL, Ty_Void());
			}
			return expTy(Tr_if(test.exp, then.exp, elsee.exp), actual_ty(then.ty));
		}

		case A_letExp: {
			A_decList d = e->u.let.decs;
			S_beginScope(venv);
			S_beginScope(tenv);
			Tr_expList decs = transDecList(lev, venv, tenv, d), i;
			struct expty body = transExp(lev, venv, tenv, e->u.let.body);
			if(decs == NULL) {
				decs = Tr_ExpList(body.exp, NULL);
			}
			else {
				i = decs;
				while(i->tail) i = i->tail;
				i->tail = Tr_ExpList(body.exp, NULL);
			}
			S_endScope(tenv);
			S_endScope(venv);
			return expTy(Tr_seq(decs), actual_ty(body.ty));
		}

		case A_assignExp: {
			struct expty exp = transExp(lev, venv, tenv, e->u.assign.exp);
			struct expty lvalue = transVar(lev, venv, tenv, e->u.assign.var);
			exp.ty = actual_ty(exp.ty);
			lvalue.ty = actual_ty(lvalue.ty);
			if((lvalue.ty->kind == Ty_record) & (exp.ty->kind == Ty_nil))
				return expTy(Tr_assign(lvalue.exp, exp.exp), Ty_Void());
			if(lvalue.read_only) {
				EM_error(e->pos, "cannot assign read-only variable");
				return expTy(NULL, Ty_Void());
			}
			if(exp.ty != lvalue.ty) {
				EM_error(e->pos, "assign expression type mismatch, have \"%s\" and \"%s\"",
						str_ty[lvalue.ty->kind],
						str_ty[exp.ty->kind]);
				return expTy(NULL, Ty_Void());
			}
         	if((lvalue.ty->kind != Ty_record) & (exp.ty->kind == Ty_nil)) {
         		EM_error(e->pos, "cannot assign nil to non-record type");
         		return expTy(NULL, Ty_Void());
         	}
         	return expTy(Tr_assign(lvalue.exp, exp.exp), Ty_Void());
		}

		case A_seqExp: {
			A_expList exps = e->u.seq;
			Tr_expList head = NULL, tail = NULL;
			if(exps == NULL)
				return expTy(Tr_Nop(), Ty_Void());
			for(; exps->tail; exps = exps->tail) {
				struct expty exp = transExp(lev, venv, tenv, exps->head);
				if(head == NULL)
					head = tail = Tr_ExpList(exp.exp, NULL);
				else
					tail = tail->tail = Tr_ExpList(exp.exp, NULL);
			}
			struct expty last = transExp(lev, venv, tenv, exps->head);
			Tr_exp a =Tr_Eseq(head, last.exp);
			return expTy(Tr_Eseq(head, last.exp), actual_ty(last.ty));
		}

        case A_arrayExp: {
        	struct expty size = transExp(lev, venv, tenv, e->u.array.size);
        	struct expty init = transExp(lev, venv, tenv, e->u.array.init);
        	Ty_ty typ = S_look(tenv, e->u.array.typ);
        	typ = actual_ty(typ);
        	size.ty = actual_ty(size.ty);
        	init.ty = actual_ty(init.ty);
        	if(typ == NULL) {
        		EM_error(e->pos, "array type undeclared");
        		return expTy(NULL,Ty_Int());
        	}
        	if(size.ty->kind != Ty_int) {
        		EM_error(e->u.array.size->pos, "array size must be int");
        		return expTy(NULL,Ty_Int());
        	}
        	typ->u.array = actual_ty(typ->u.array);
        	if(init.ty != actual_ty(typ->u.array)) {
        		if(init.ty->kind == Ty_nil &&
        			(typ->u.array->kind == Ty_record ||
        			typ->u.array->kind == Ty_array)) {
        			return expTy(Tr_array(size.exp, init.exp), typ);
        		}
        		EM_error(e->u.array.init->pos,
        				"array initialization type mismatch, \"%s\" and \"%s\"",
						str_ty[init.ty->kind],
						str_ty[typ->u.array->kind]);
        		return expTy(NULL,Ty_Int());
        	}
        	return expTy(Tr_array(size.exp, init.exp), typ);
        }

        case A_recordExp: {
        	A_efieldList efields = e->u.record.fields;
        	Ty_ty typ = S_look(tenv, e->u.record.typ);
        	Tr_expList l = NULL, il;
        	int size = 0;
        	typ = actual_ty(typ);
        	if(typ) {
        		Ty_fieldList fields = typ->u.record;
        		for(; fields; fields = fields->tail) size++;
        		fields = typ->u.record;
        		if( typ->kind != Ty_record) {
        			EM_error(e->pos,"in type \"%s\": not a record", S_name(e->u.record.typ));
        			return expTy(NULL, Ty_Int());
        		}
        		while(fields) {
        			if(efields == NULL) {
        				EM_error(e->pos, "in type \"%s\": structure mismatch",
        						S_name(e->u.record.typ));
        				return expTy(NULL, Ty_Int());
        			}
        			fields->head->ty = actual_ty(fields->head->ty);
        			struct expty efield = transExp(lev, venv, tenv, efields->head->exp);
        			if(l == NULL) {
        				l = il = Tr_ExpList(efield.exp, NULL);
        			}
        			else {
        				il->tail = Tr_ExpList(efield.exp, NULL);
        				il = il->tail;
        			}
        			Ty_ty ty_exp = actual_ty(efield.ty);
        			if(fields->head->ty != ty_exp) {
        				struct expty et = transExp(lev, venv, tenv, efields->head->exp);
        				if(actual_ty(fields->head->ty)->kind == Ty_record &&
        						et.ty->kind == Ty_nil) {
        					fields = fields->tail;
        					efields = efields->tail;
        					continue;
        				}
        				EM_error(efields->head->exp->pos,
        						"in type \"%s\": field type mismatch, have \"%s\" and \"%s\"",
        						S_name(e->u.record.typ), str_ty[fields->head->ty->kind],
        						str_ty[ty_exp->kind]);
        				return expTy(NULL,Ty_Int());
        			}
        			fields = fields->tail;
        			efields = efields->tail;
        		}
        		if(efields) {
        			EM_error(efields->head->exp->pos, "in type \"%s\":too many fields",
        					S_name(e->u.record.typ));
        			return expTy(NULL,Ty_Int());
        		}
        		return expTy(Tr_record(size, l), actual_ty(typ));
        	}
        	else
        	{
        		EM_error(e->pos,"cannot find record: %s",
        				S_name(e->u.record.typ));
        		return expTy(NULL,Ty_Int());
        	}
        }
		/* TODO: Adicionar sobrecarga para matriz
		 * TODO: Adicionar sobrecarga para comparação de vetor e matriz
		 *
		 */
		case A_opExp: {
			struct expty left = transExp(lev, venv, tenv, e->u.op.left);
			struct expty right = transExp(lev, venv, tenv, e->u.op.right);
			switch(e->u.op.oper) {
				//sobrecarga
				case A_plusOp: {
					if((left.ty->kind == Ty_nil) & (right.ty->kind == Ty_nil)) {
						EM_error(e->pos, "cannot operate nil expression");
						return expTy(NULL, Ty_Int());
					}
					if((left.ty->kind == Ty_string) & (right.ty->kind == Ty_string)) {
						return expTy(overloadCall("_strConcat", left.exp, right.exp),
								left.ty);
					}
					if((left.ty->kind == Ty_array) & (right.ty->kind == Ty_array)) {
						return expTy(overloadCall("_arrayAppend", left.exp, right.exp),
								left.ty);
					}
				}
				case A_minusOp: {
					if((left.ty->kind == Ty_nil) & (right.ty->kind == Ty_nil)) {
						EM_error(e->pos, "cannot operate nil expression");
						return expTy(NULL, Ty_Int());
					}
					if((left.ty->kind == Ty_string) & (right.ty->kind == Ty_string)) {
						return expTy(overloadCall("_strRemove", left.exp, right.exp),
								left.ty);
					}
					if((left.ty->kind == Ty_array) & (right.ty->kind == Ty_array)) {
						return expTy(overloadCall("_arrayRemove", left.exp, right.exp),
								left.ty);
					}
				}
				case A_timesOp: {
					if((left.ty->kind == Ty_nil) & (right.ty->kind == Ty_nil)) {
						EM_error(e->pos, "cannot operate nil expression");
						return expTy(NULL, Ty_Int());
					}
					if((left.ty->kind == Ty_string) & (right.ty->kind == Ty_int)) {
						return expTy(overloadCall("_strMultiply", left.exp, right.exp),
								left.ty);
					}
					if((left.ty->kind == Ty_array) & (right.ty->kind == Ty_int)) {
						return expTy(overloadCall("_arrayMultiply", left.exp, right.exp),
								left.ty);
					}
				}
				case A_divideOp: {
					if((left.ty->kind == Ty_nil) & (right.ty->kind == Ty_nil)) {
						EM_error(e->pos, "cannot operate nil expression");
						return expTy(NULL, Ty_Int());
					}
					if((left.ty->kind == Ty_string) & (right.ty->kind == Ty_string)) {
						return expTy(overloadCall("_strRemove", left.exp, right.exp),
								left.ty);
					}
					if((left.ty->kind == Ty_array) & (right.ty->kind == Ty_array)) {
						return expTy(overloadCall("_arrayRemove", left.exp, right.exp),
								left.ty);
					}
					if((left.ty->kind != Ty_int) | (right.ty->kind != Ty_int)) {
						EM_error(e->pos, "op expression type mismatch, have \"%s\" and \"%s\"",
							str_ty[left.ty->kind],
							str_ty[right.ty->kind]);
						return expTy(NULL, Ty_Int());
					}
					if(left.ty != right.ty) {
						EM_error(e->pos, "op expression type mismatch, have \"%s\" and \"%s\"",
								str_ty[left.ty->kind],
								str_ty[right.ty->kind]);
						return expTy(NULL, Ty_Int());
					}
					return expTy(
							Tr_binop(e->u.op.oper, left.exp, right.exp),
							Ty_Int());
				}
				case A_eqOp:
					if((left.ty->kind == Ty_string) & ((right.ty->kind == Ty_string) | (right.ty->kind == Ty_nil))) {
						return expTy(overloadCall("_strCompare", left.exp, right.exp),
								Ty_Int());
					}
				case A_neqOp: {
					if((left.ty->kind == Ty_string) & (right.ty->kind == Ty_string)) {
						return expTy(Tr_relop(T_eq,
									overloadCall("_strCompare", left.exp, right.exp),
									Tr_int(0)),
									Ty_Int());
					}
					if(left.ty != right.ty) {
						if((left.ty->kind == Ty_nil) & (right.ty->kind == Ty_record)) {
							return expTy(
									Tr_relop(e->u.op.oper - 4, left.exp, right.exp),
									Ty_Int());
						}
						if((left.ty->kind == Ty_record) & (right.ty->kind == Ty_nil)) {
							return expTy(
									Tr_relop(e->u.op.oper - 4, left.exp, right.exp),
									Ty_Int());
						}
						EM_error(e->pos, "op expression type mismatch, have \"%s\" and \"%s\"",
							str_ty[left.ty->kind],
							str_ty[right.ty->kind]);
						return expTy(NULL, Ty_Int());
					}
					return expTy(
							Tr_relop(e->u.op.oper - 4, left.exp, right.exp),
							Ty_Int());
				}
				case A_ltOp:
				case A_leOp:
				case A_gtOp:
				case A_geOp: {
					if((left.ty->kind == Ty_nil) & (right.ty->kind == Ty_nil)) {
						EM_error(e->pos, "cannot operate nil expression");
						return expTy(NULL, Ty_Int());
					}
					if(left.ty != right.ty) {
						EM_error(e->pos, "op expression type mismatch, have \"%s\" and \"%s\"",
								str_ty[left.ty->kind],
								str_ty[right.ty->kind]);
						return expTy(NULL, Ty_Int());
					}
					if((left.ty->kind != Ty_int) | (right.ty->kind != Ty_int)) {
						EM_error(e->pos, "op expression type mismatch, have \"%s\" and \"%s\"",
							str_ty[left.ty->kind],
							str_ty[right.ty->kind]);
						return expTy(NULL, Ty_Int());
					}
					return expTy(
							Tr_relop(e->u.op.oper - 4, left.exp, right.exp),
							Ty_Int());
				}
			}
		}
	}
}

Ty_tyList MakeTyList(S_table tenv, A_fieldList params) {
	if(params) {
		Ty_ty t = S_look(tenv, params->head->typ);
		if(t) {
			return Ty_TyList(t, MakeTyList(tenv, params->tail));
		}
		else {
			EM_error(params->head->pos, "in type: param type unkown");
			return NULL;
		}
	}
	else return NULL;
}

U_boolList MakeBoolList(A_fieldList params) {
	if(params) {
		return U_BoolList(params->head->escape, MakeBoolList(params->tail));
	}
	else return NULL;
}

Tr_exp transDec(Tr_level lev, S_table venv, S_table tenv, A_dec d) {
	switch(d->kind) {
		case A_varDec: {
			struct expty e = transExp(lev, venv, tenv , d->u.var.init);
			Tr_access l = Tr_allocLocal(lev, d->u.var.escape);
			if(d->u.var.typ) { //tipo explicito
                Ty_ty var_ty = S_look(tenv, d->u.var.typ);
                var_ty = actual_ty(var_ty);
             	if((var_ty->kind != Ty_record) & (e.ty->kind == Ty_nil)) {
             		EM_error(d->pos, "cannot assign nil to non-record type");
             		return NULL;
             	}
             	//estrutura igual
                if(((var_ty->kind != e.ty->kind)) && ((var_ty->kind != Ty_record) & (e.ty->kind != Ty_nil))){
                	EM_error(d->pos, "var initialization expression type mismatch, have \"%s\" and \"%s\"",
						str_ty[e.ty->kind],
						str_ty[var_ty->kind]);
                	return NULL;
                }

                S_enter(venv, d->u.var.var, E_VarEntry(l, var_ty, FALSE));
                return Tr_varDec(l, lev, e.exp);
			}
			else if(e.ty->kind == Ty_nil) {
				EM_error(d->pos, "cannot infer variable type from nil expression");
				return NULL;
			}
			S_enter(venv, d->u.var.var, E_VarEntry(l, actual_ty(e.ty), FALSE));
			return Tr_varDec(l, lev, e.exp);
		}

		case A_typeDec: {
			A_nametyList typ = d->u.type;
			A_nametyList typ_next = d->u.type->tail;
			//verifica se tem um tipo com o mesmo nome adjacente
			while (typ) {
				typ = typ->tail;
				if(!typ) break;
				typ_next = typ_next->tail;
				if(!typ_next) break;
				if(typ->head->name == typ_next->head->name) {
					EM_error(d->pos, "adjacent types with same name");
					return;
				}
			}
			//gera os tipos
			for(typ = d->u.type; typ; typ = typ->tail) {
				S_enter(tenv, typ->head->name, Ty_Name(typ->head->name, NULL));
			}
			//resolve os tipos
			for(typ = d->u.type; typ; typ = typ->tail) {
				Ty_ty t = S_look(tenv, typ->head->name);
				t->u.name.ty = transTy(lev, tenv, typ->head->ty);
			}
			//verificação de ciclo
			for(typ = d->u.type; typ; typ = typ->tail) {
				Ty_ty head = S_look(tenv, typ->head->name);
				Ty_ty it = head->u.name.ty;
				while((head->kind == Ty_name) & (it->kind == Ty_name)) {
					if(it == head) {
						EM_error(d->pos, "invalid type reference cicle");
						return;
					}
					it = it->u.name.ty;
				}
			}
			return Tr_int(0);
		}

		case A_functionDec: {
			A_fundecList fun = d->u.function;
			A_fundecList fun_next = d->u.function->tail;
			//verifica se tem uma fç com o mesmo nome adjacente
			while (fun) {
				fun = fun->tail;
				if(!fun) break;
				fun_next = fun_next->tail;
				if(!fun_next) break;
				if(fun->head->name == fun_next->head->name) {
					EM_error(fun_next->head->pos, "adjacent functions with same name");
					return;
				}
			}
			//insere a fç na tabela de simbolos
			for(fun = d->u.function; fun; fun = fun->tail) {
				Ty_ty result;
				if(fun->head->result) result = S_look(tenv, fun->head->result);
				else result = Ty_Void();
				Ty_tyList tylist = MakeTyList(tenv, fun->head->params);
				U_boolList blist = MakeBoolList(fun->head->params);
				Temp_label label = fun->head->name;
				Tr_level new_level = Tr_newLevel(lev, label, blist);
				S_enter(venv, fun->head->name, E_FunEntry(new_level, label, tylist, result));
			}
			//verificação semantica da fç tem que ser em outro for para
			//fçs mutuamente recursivas; o outro for coloca as fçs na tabela de simbolos
			for(fun = d->u.function; fun; fun = fun->tail) {
				S_beginScope(venv);
				Ty_tyList tylist = MakeTyList(tenv, fun->head->params);
				Ty_tyList tys;
				A_fieldList param = fun->head->params;
				E_enventry ev = S_look(venv, fun->head->name);
				Tr_accessList formals = Tr_formals(ev->u.fun.level);
				for(tys = tylist; tys; tys = tys->tail) {
					S_enter(venv, param->head->name,
							E_VarEntry(formals->head, actual_ty(tys->head), FALSE));
					param = param->tail;
					formals = formals->tail;
				}
				struct expty body = transExp(ev->u.fun.level, venv, tenv, fun->head->body);
				Tr_procEntryExit(ev->u.fun.level, body.exp, Tr_formals(ev->u.fun.level));
				S_endScope(venv);
				//verificacao de retorno e tipo
				if(fun->head->result) {
					Ty_ty fun_ty = S_look(tenv, fun->head->result);
					fun_ty = actual_ty(fun_ty);
					if(body.ty != fun_ty) {
					EM_error(fun->head->pos,
						"in function %s: return type mismatch, have \"%s\" and \"%s\"",
						S_name(fun->head->name),
						str_ty[fun_ty->kind],
						str_ty[body.ty->kind]);
						return;
					}
				}
				else if(body.ty->kind != Ty_void) {
					EM_error(fun->head->pos,"in function %s: procedure returns value",
							S_name(fun->head->name));
					return;
				}
			}
			return Tr_int(0);
		}
	}
}

Ty_ty transTy(Tr_level lev, S_table tenv,  A_ty a) {
	switch(a->kind) {
		case A_nameTy: {
			Ty_ty typ = S_look(tenv, a->u.name);
			if(typ) {
				return typ;
			}
			else {
				EM_error(a->pos, "type \"%s\" undeclared", S_name(a->u.name));
				return Ty_Int();
			}
		}

		case A_arrayTy: {
			Ty_ty t = S_look(tenv, a->u.array);
			if(!t) {
				EM_error(a->pos, "type \"%s\" undefined", S_name(a->u.array));
				return Ty_Int();
			}
			if(t->kind == Ty_nil) {
				EM_error(a->pos, "invalid array type");
				return Ty_Int();
			}
			return Ty_Array(t);
		}

		case A_recordTy: {
            A_fieldList fields;
            Ty_fieldList reverse = NULL, forward = NULL;
            for(fields = a->u.record; fields; fields = fields->tail) {
                Ty_ty typ = S_look(tenv, fields->head->typ);
                if(typ == NULL) {
                    EM_error(fields->head->pos, "type \"%s\" undefined", S_name(fields->head->typ));
                    return Ty_Int();
                }
                reverse = Ty_FieldList(Ty_Field(fields->head->name, typ), reverse);
            }
            for(;reverse; reverse = reverse->tail)
                forward = Ty_FieldList(reverse->head, forward);
            return Ty_Record(forward);
		}
	}
}

F_fragList SEM_transProg(A_exp exp) {
	S_table tenv = E_base_tenv();
	S_table venv = E_base_venv();
	Tr_level level = Tr_outermost();
	Esc_findEscape(exp);
	struct expty main = transExp(level, venv, tenv, exp);
	Tr_procEntryExit(level, main.exp, Tr_formals(level));
	return Tr_getResult();
}
