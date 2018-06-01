/*
 * mipscodegen.c - Functions to translate to Assem-instructions for
 *             the Jouette assembly language using Maximal Munch.
 */

#include <stdio.h>
#include <stdlib.h> /* for atoi */
#include <string.h> /* for strcpy */
#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "tree.h"
#include "assem.h"
#include "frame.h"
#include "errormsg.h"

AS_targets AS_Targets(Temp_labelList labels) {
   AS_targets p = checked_malloc (sizeof *p);
   p->labels=labels;
   return p;
}

AS_instr AS_Oper(string a, Temp_tempList d, Temp_tempList s, AS_targets j) {
  AS_instr p = (AS_instr) checked_malloc (sizeof *p);
  p->kind = I_OPER;
  p->u.OPER.assem=String(a);
  p->u.OPER.dst=d; 
  p->u.OPER.src=s; 
  p->u.OPER.jumps=j;
  p->u.OPER.cjump=FALSE;
  return p;
}

AS_instr AS_Label(string a, Temp_label label) {
  AS_instr p = (AS_instr) checked_malloc (sizeof *p);
  p->kind = I_LABEL;
  p->u.LABEL.assem=String(a);
  p->u.LABEL.label=label; 
  return p;
}

AS_instr AS_Move(string a, Temp_tempList d, Temp_tempList s) {
  AS_instr p = (AS_instr) checked_malloc (sizeof *p);
  p->kind = I_MOVE;
  p->u.MOVE.assem=String(a);
  p->u.MOVE.dst=d; 
  p->u.MOVE.src=s; 
  return p;
}

AS_instrList AS_InstrList(AS_instr head, AS_instrList tail)
{AS_instrList p = checked_malloc (sizeof *p);
 p->head=head; p->tail=tail;
 return p;
}

/* put list b at the end of list a */
AS_instrList AS_splice(AS_instrList a, AS_instrList b) {
  AS_instrList p;
  if (a==NULL) return b;
  for(p=a; p->tail!=NULL; p=p->tail) ;
  p->tail=b;
  return a;
}
	
static Temp_temp nthTemp(Temp_tempList list, int i) {
  assert(list);
  if (i==0) return list->head;
  else return nthTemp(list->tail,i-1);
}

static Temp_label nthLabel(Temp_labelList list, int i) {
  assert(list);
  if (i==0) return list->head;
  else return nthLabel(list->tail,i-1);
}


/* first param is string created by this function by reading 'assem' string
 * and replacing `d `s and `j stuff.
 * Last param is function to use to determine what to do with each temp.
 */
static void format(char *result, string assem, 
		Temp_tempList dst, Temp_tempList src,
		   AS_targets jumps, Temp_map m)
{
  char *p;
  int i = 0; /* offset to result string */
  for(p = assem; p && *p != '\0'; p++)
    if (*p == '`')
      switch(*(++p)) {
      case 's': {int n = atoi(++p);
		 string s = Temp_look(m, nthTemp(src,n));
		 strcpy(result+i, s);
		 i += strlen(s);
	       }
	break;
      case 'd': {int n = atoi(++p);
		 string s = Temp_look(m, nthTemp(dst,n));
		 strcpy(result+i, s);
		 i += strlen(s);
	       }
	break;
      case 'j': assert(jumps); 
	       {int n = atoi(++p);
		 string s = Temp_labelstring(nthLabel(jumps->labels,n));
		 strcpy(result+i, s);
		 i += strlen(s);
	       }
	break;
      case '`': result[i] = '`'; i++; 
	break;
      default: assert(0);
      }
    else {result[i] = *p; i++; }
  result[i] = '\0';
}


void AS_print(FILE *out, AS_instr i, Temp_map m)
{
  char r[200]; /* result */
  switch (i->kind) {
  case I_OPER:
    format(r, i->u.OPER.assem, i->u.OPER.dst, i->u.OPER.src, i->u.OPER.jumps, m);
    fprintf(out, "%s", r);
    break;
  case I_LABEL:
    format(r, i->u.LABEL.assem, NULL, NULL, NULL, m); 
    fprintf(out, "%s", r); 
    /* i->u.LABEL->label); */
    break;
  case I_MOVE:
    format(r, i->u.MOVE.assem, i->u.MOVE.dst, i->u.MOVE.src, NULL, m);
    fprintf(out, "%s", r);
    break;
  }
}

/* c should be COL_color; temporarily it is not */
void AS_printInstrList (FILE *out, AS_instrList iList, Temp_map m)
{
  for (; iList; iList=iList->tail) {
    AS_print(out, iList->head, m);
  }
}

AS_proc AS_Proc(string p, AS_instrList b, string e)
{AS_proc proc = checked_malloc(sizeof(*proc));
 proc->prolog=p; proc->body=b; proc->epilog=e;
 return proc;
}

bool uselessMove(AS_instr i) {
	if(i->kind == I_MOVE)
		return i->u.MOVE.dst->head == i->u.MOVE.src->head;
	else
		return FALSE;
}

AS_instrList AS_clean(AS_instrList il) {
	AS_instrList i, ret, tail;
	for(i = il; i; i = i->tail) {
		if(!uselessMove(i->head)) {
			if(ret == NULL)
				ret = tail = AS_InstrList(i->head, NULL);
			else
				tail = tail->tail = AS_InstrList(i->head, NULL);
		}
	}
	return ret;
}

bool AS_in(AS_instrList list, AS_instr item) {
	if(list == NULL)
		return FALSE;
	else if(list->head == item)
		return TRUE;
	else
		return AS_in(list->tail, item);
}

AS_instrList AS_minus(AS_instrList a, AS_instrList b) {
	AS_instrList ret = NULL, i;
	for(i = a; i; i = i->tail)
		if(!AS_in(b, i->head))
			ret = AS_InstrList(i->head, ret);
	return ret;
}

AS_instrList AS_copy(AS_instrList a) {
	if(a == NULL)
		return NULL;
	else
		return AS_InstrList(a->head, AS_copy(a->tail));
}

AS_instrList AS_union(AS_instrList a, AS_instrList b) {
	AS_instrList ret = AS_copy(a), i;
	for(i = b; i; i = i->tail)
		if(!AS_in(ret, i->head))
			ret = AS_InstrList(i->head, ret);
	return ret;
}

AS_instrList AS_intersect(AS_instrList a, AS_instrList b) {
	AS_instrList ret = NULL, i;
	for(i = a; i; i = i->tail)
		if(AS_in(b, i->head))
			ret = AS_InstrList(i->head, ret);
	return ret;
}

bool AS_compare(AS_instrList a, AS_instrList b) {
	AS_instrList i, j;
	if(a == b)
		return TRUE;
	for(i = a; i; i = i->tail)
		if(!AS_in(b, i->head))
			return FALSE;
	for(j = b; j; j = j->tail)
		if(!AS_in(a, j->head))
			return FALSE;
	return TRUE;
}
