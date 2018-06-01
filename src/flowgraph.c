#include "util.h"
#include "table.h"
#include "temp.h"
#include "assem.h"
#include "flowgraph.h"
#include "graph.h"

TAB_table def = NULL, use = NULL;
TAB_table nodes = NULL;

Temp_tempList FG_def(AS_instr n) {
    return (Temp_tempList)TAB_look(def, n);
}

Temp_tempList FG_use(AS_instr n) {
    return (Temp_tempList)TAB_look(use, n);
}

AS_instr FG_getInstr(G_node n) {
    return G_look(nodes, n);
}

Temp_tempList getDef(AS_instr i) {
	switch(i->kind) {
		case I_OPER:
			return i->u.OPER.dst;
		case I_MOVE:
			return i->u.MOVE.dst;
	}
	return NULL;
}

Temp_tempList getUse(AS_instr i) {
	switch(i->kind) {
		case I_OPER:
			return i->u.OPER.src;
		case I_MOVE:
			return i->u.MOVE.src;
	}
	return NULL;
}

void printnode(void * info) {
	AS_instr instr = (AS_instr)info;
	AS_print(stdout, instr, Temp_name());
}

G_graph FG_AssemFlowGraph(AS_instrList il) {
    G_graph graph = G_Graph();
    TAB_table labels = TAB_empty();
    use = TAB_empty();
    def = TAB_empty();
    nodes = TAB_empty();
    AS_instrList instr;
    for(instr = il; instr; instr = instr->tail) {
        G_node new = G_Node(graph, instr->head);
        TAB_enter(nodes, instr->head, new);
        if (instr->head->kind == I_LABEL)
            TAB_enter(labels, instr->head->u.LABEL.label, new);
    }
    for(instr = il; instr->tail; instr = instr->tail) {
    	AS_instr i = instr->head;
    	TAB_enter(def, i, getDef(i));
    	TAB_enter(use, i, getUse(i));
    	if(i->kind == I_OPER && i->u.OPER.jumps) {
    		Temp_labelList temp;
    		for(temp = i->u.OPER.jumps->labels; temp; temp = temp->tail) {
    			G_node from = TAB_look(nodes, i);
    			G_node to = TAB_look(labels, temp->head);
    			G_addEdge(from, to);
    		}
    		if(i->u.OPER.cjump) {
    			G_node from = TAB_look(nodes, i);
    			G_node to = TAB_look(nodes, instr->tail->head);
    			G_addEdge(from, to);
    		}
    	}
    	else {
			G_node from = TAB_look(nodes, i);
			G_node to = TAB_look(nodes, instr->tail->head);
			G_addEdge(from, to);
    	}
    }
    //G_show(stdout, G_nodes(graph), printnode);
    return graph;
}
