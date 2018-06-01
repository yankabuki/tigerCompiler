#ifndef H_ESCAPE_H
#define H_ESCAPE_H

#include "absyn.h"

typedef struct _escapeEntry * EscapeEntry;
struct _escapeEntry {
	enum {E_field, E_dec} kind;
	int depth;
	union {
		A_field field;
		A_dec dec;
	} u;
};

EscapeEntry E_EscapeDec(int depth, A_dec dec);
EscapeEntry E_EscapeField(int depth, A_field field);
void Esc_findEscape(A_exp exp);
#endif
