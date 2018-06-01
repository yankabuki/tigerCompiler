#ifndef H_TEMP_H
#define H_TEMP_H
/*
 * temp.h 
 *
 */
#include <stdio.h>
#include "util.h"
#include "symbol.h"

struct Temp_temp_ {int num;};
typedef struct Temp_temp_ *Temp_temp;
Temp_temp Temp_newtemp(void);
Temp_temp Temp_newregister(string r);

typedef struct Temp_tempList_ *Temp_tempList;
struct Temp_tempList_ { Temp_temp head; Temp_tempList tail;};
Temp_tempList Temp_TempList(Temp_temp h, Temp_tempList t);

typedef S_symbol Temp_label;
void Temp_reset(void);
Temp_label Temp_newlabel(void);
Temp_label Temp_namedlabel(string name);
void Temp_reset(void);
int Temp_count(void);
string Temp_labelstring(Temp_label s);

typedef struct Temp_labelList_ *Temp_labelList;
struct Temp_labelList_ { Temp_label head; Temp_labelList tail;};
Temp_labelList Temp_LabelList(Temp_label h, Temp_labelList t);

typedef struct Temp_map_ *Temp_map;
Temp_map Temp_empty(void);
Temp_map Temp_layerMap(Temp_map over, Temp_map under);
void Temp_enter(Temp_map m, Temp_temp t, string s);
string Temp_look(Temp_map m, Temp_temp t);
void Temp_dumpMap(FILE *out, Temp_map m);

Temp_map Temp_name(void);
bool Temp_in(Temp_tempList list, Temp_temp item);
Temp_tempList Temp_minus(Temp_tempList a, Temp_tempList b);
Temp_tempList Temp_copy(Temp_tempList a);
Temp_tempList Temp_union(Temp_tempList a, Temp_tempList b);
bool Temp_compare(Temp_tempList a, Temp_tempList b);
int Temp_listCount(Temp_tempList a);
void Temp_printList(Temp_tempList list);
#endif
