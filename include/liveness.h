#ifndef H_LIVENESS_H
#define H_LIVENESS_H

Temp_tempList Live_in(AS_instr node);
Temp_tempList Live_out(AS_instr node);
void Live_liveness(G_graph flow);
#endif
