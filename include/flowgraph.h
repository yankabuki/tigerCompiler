#ifndef H_FLOWGRAPH_H
#define H_FLOWGRAPH_H

#include "util.h"
#include "temp.h"
#include "assem.h"
#include  "graph.h"
/*
 * flowgraph.h - Function prototypes to represent control flow graphs.
 */
Temp_tempList FG_def(AS_instr n);
Temp_tempList FG_use(AS_instr n);
bool FG_isMove(G_node n);
AS_instr FG_getInstr(G_node n);
G_graph FG_AssemFlowGraph(AS_instrList il);
#endif
