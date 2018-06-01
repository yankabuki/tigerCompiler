#include "env.h"
#include "escape.h"

static void traverseExp(S_table env, int depth, A_exp e);
static void traverseDec(S_table env, int depth, A_dec d);
static void traverseVar(S_table env, int depth, A_var v);

EscapeEntry E_EscapeDec(int depth, A_dec dec) {
	EscapeEntry ev = checked_malloc(sizeof(*ev));
	ev->depth = depth;
	ev->u.dec = dec;
	ev->kind = E_dec;
	return ev;
}

EscapeEntry E_EscapeField(int depth, A_field field) {
	EscapeEntry ev = checked_malloc(sizeof(*ev));
	ev->depth = depth;
	ev->u.field = field;
	ev->kind = E_field;
	return ev;
}

void Esc_findEscape(A_exp exp) {
	S_table env = S_empty();
	int depth = 0;
	traverseExp(env, depth, exp);
}

static void traverseExp(S_table env, int depth, A_exp e) {
	switch(e->kind) {
		case A_nilExp:
		case A_intExp:
		case A_stringExp:
		case A_breakExp:
			break;

		case A_varExp: {
			traverseVar(env, depth, e->u.var);
			break;
		}

		case A_opExp: {
			traverseExp(env, depth, e->u.op.left);
			traverseExp(env, depth, e->u.op.right);
			break;
		}

		case A_callExp: {
			A_expList arg;
			for(arg = e->u.call.args; arg; arg = arg->tail) {
				traverseExp(env, depth, arg->head);
			}
			break;
		}

		case A_recordExp: {
			A_efieldList efield;
			for(efield = e->u.record.fields; efield; efield = efield->tail) {
				traverseExp(env, depth, efield->head->exp);
			}
			break;
		}

		case A_seqExp: {
			A_expList seq;
			for(seq = e->u.seq; seq; seq = seq->tail) {
				traverseExp(env, depth, seq->head);
			}
			break;
		}

		case A_assignExp: {
			traverseVar(env, depth, e->u.assign.var);
			traverseExp(env, depth, e->u.assign.exp);
			break;
		}

		case A_ifExp: {
			traverseExp(env, depth, e->u.iff.test);
			traverseExp(env, depth, e->u.iff.then);
			if(e->u.iff.elsee)
				traverseExp(env, depth, e->u.iff.elsee);
			break;
		}

		case A_whileExp: {
			traverseExp(env, depth, e->u.whilee.test);
			traverseExp(env, depth, e->u.whilee.body);
			break;
		}

		case A_forExp: {
			traverseExp(env, depth, e->u.forr.lo);
			traverseExp(env, depth, e->u.forr.hi);
			traverseExp(env, depth, e->u.forr.body);
            break;
		}

		case A_letExp: {
            S_beginScope(env);
            A_decList dec;
            for (dec = e->u.let.decs; dec; dec = dec->tail) {
            	traverseDec(env, depth, dec->head);
            }
			traverseExp(env, depth, e->u.let.body);
            S_endScope(env);
            break;
		}

		case A_arrayExp:
			traverseExp(env, depth, e->u.array.size);
			traverseExp(env, depth, e->u.array.init);
			break;
	}
}

static void traverseDec(S_table env, int depth, A_dec d) {
	switch(d->kind) {
		case A_functionDec: {
			A_fundecList func;
			for (func = d->u.function; func; func = func->tail) {
				A_fieldList param;
				depth++;
				S_beginScope(env);
				for(param = func->head->params; param; param = param->tail) {
					A_field p = param->head;
					S_enter(env, p->name, E_EscapeField(depth, p));
				}
				traverseExp(env, depth, func->head->body);
				S_endScope(env);
				depth--;
			}
			break;
		}

		case A_varDec: {
			S_enter(env, d->u.var.var, E_EscapeDec(depth, d));
			traverseExp(env, depth, d->u.var.init);
		}

		case A_typeDec:
			break;
	}
}

static void traverseVar(S_table env, int depth, A_var v) {
    switch (v->kind) {
    	case A_simpleVar: {
    		EscapeEntry entry = S_look(env, v->u.simple);
            if (entry && entry->depth < depth) {
            	if(entry->kind == E_field) {
            		entry->u.field->escape = TRUE;
            	}
            	else {
            		entry->u.dec->u.var.escape = TRUE;
            	}
            }
            break;
    	}

    	case A_fieldVar: {
            traverseVar(env, depth, v->u.field.var);
            break;
    	}
    	case A_subscriptVar: {
            traverseVar(env, depth, v->u.subscript.var);
            traverseExp(env, depth, v->u.subscript.exp);
            break;
    	}
    }
}
