#include "table.h"
#include "temp.h"
#include "graph.h"
#include "flowgraph.h"
#include "assem.h"
#include "liveness.h"

static void enterLiveMap(G_table t, G_node flowNode, Temp_tempList temps) {
	G_enter(t, flowNode, temps);
}

static Temp_tempList lookLiveMap(G_table t, G_node flowNode) {
	return (Temp_tempList)G_look(t, flowNode);
}

static Temp_tempList * ins, *outs;
static TAB_table in, out;

Temp_tempList Live_in(AS_instr node) {
	return (Temp_tempList) TAB_look(in, node);
}

Temp_tempList Live_out(AS_instr node) {
	return (Temp_tempList) TAB_look(out, node);
}

void Live_liveness(G_graph flow) {
	G_nodeList nodes;
	ins = checked_malloc(sizeof(*ins) * flow->nodecount);
	outs = checked_malloc(sizeof(*outs) * flow->nodecount);
	in = TAB_empty();
	out = TAB_empty();
    //liveMap usando o algoritmo de conjuntos do livro
    for(nodes = G_nodes(flow); nodes; nodes = nodes->tail) {
    	G_node node = nodes->head;
    	ins[node->mykey] = NULL;
    	outs[node->mykey] = NULL;
    }
    bool equal;
    G_node dd;
    do {
    	equal = TRUE;
        for(nodes = G_nodes(flow); nodes; nodes = nodes->tail) {
        	G_node node = nodes->head;
        	Temp_tempList _in = Temp_copy(ins[node->mykey]);
        	Temp_tempList _out = Temp_copy(outs[node->mykey]);
        	Temp_tempList use = Temp_copy(FG_use(G_nodeInfo(node)));
        	Temp_tempList def = Temp_copy(FG_def(G_nodeInfo(node)));
        	ins[node->mykey] = Temp_union(use, Temp_minus(outs[node->mykey], def));
        	outs[node->mykey] = NULL;
        	G_nodeList i;
        	for(i = G_succ(node); i; i = i->tail) {
        		Temp_tempList succ = ins[i->head->mykey];
        		outs[node->mykey] = Temp_union(outs[node->mykey], succ);
        	}
        	equal = equal &&
        			Temp_compare(ins[node->mykey], _in) &&
        			Temp_compare(outs[node->mykey], _out);
        }
    }while(!equal);
    for(nodes = G_nodes(flow); nodes; nodes = nodes->tail) {
    	G_node node = nodes->head;
		TAB_enter(in, G_nodeInfo(node), ins[node->mykey]);
		TAB_enter(out, G_nodeInfo(node), outs[node->mykey]);
    }
}
