#include "frame.h"
#include "table.h"
#include "temp.h"
#include "assem.h"
#include "graph.h"
#include "flowgraph.h"
#include "liveness.h"
#include "regalloc.h"

static Temp_tempList spillWorklist = NULL;
static Temp_tempList freezeWorklist = NULL;
static Temp_tempList simplifyWorklist = NULL;
static Temp_tempList coalescedNodes = NULL;
static Temp_tempList spilledNodes = NULL;
static Temp_tempList coloredNodes = NULL;

static Temp_tempList precolored;
static Temp_tempList initial;

static Temp_tempList * adjList;
static Temp_temp * alias;
static Temp_temp * color;
static int * degree;
static int * uses;

static AS_instrList * moveList;
static AS_instrList worklistMoves = NULL;
static AS_instrList coalescedMoves = NULL;
static AS_instrList constrainedMoves = NULL;
static AS_instrList activeMoves = NULL;
static AS_instrList frozenMoves = NULL;

static Temp_tempList selectStack;


static Temp_tempList T(Temp_temp n) {
	return Temp_TempList(n, NULL);
}

static AS_instrList A(AS_instr a) {
	return AS_InstrList(a, NULL);
}

static void removeGraph(Temp_temp n) {
	int i;
	for(i = 0; i < Temp_count(); i++) {
		if(i == n->num)
			adjList[i] = NULL;
		else if(adjList[i])
			adjList[i] = Temp_minus(adjList[i], T(n));
	}
}

static void printGraph(void) {
	int i;
	printf("GRAPH\n");
	for(i = 0; i < Temp_count(); i++) {
		if(adjList[i]) {
			printf("%i: ", i);
			Temp_printList(adjList[i]);
			printf("\n");
		}
	}
}

static void stackPush(Temp_temp temp) {
	if(Temp_in(selectStack, temp))
		selectStack = Temp_minus(selectStack, T(temp));
	selectStack = Temp_TempList(temp, selectStack);
}

static Temp_temp stackPop(void) {
	Temp_temp r = selectStack->head;
	selectStack = selectStack->tail;
	return r;
}

static bool isMoveInstruction(AS_instr node) {
	return(node->kind == I_MOVE);
}

static AS_instrList NodeMoves(Temp_temp n) {
	return AS_intersect(moveList[n->num], AS_union(activeMoves, worklistMoves));
}

static bool MoveRelated(Temp_temp n) {
	return NodeMoves(n) != NULL;
}

static Temp_tempList Adjacent(Temp_temp n) {
	return Temp_minus(adjList[n->num], Temp_union(selectStack, coalescedNodes));
}

static void AddEdge(Temp_temp u, Temp_temp v) {
	if(u != v) {
		if(!Temp_in(adjList[u->num], v)) {
			degree[u->num]++;
			adjList[u->num] = Temp_union(adjList[u->num], T(v));
		}
		if(!Temp_in(adjList[v->num], u)) {
			adjList[v->num] = Temp_union(adjList[v->num], T(u));
			degree[v->num]++;
		}
	}
}

void RA_initUses(void) {
	spillWorklist = NULL;
	freezeWorklist = NULL;
	simplifyWorklist = NULL;
	coalescedNodes = NULL;
	spilledNodes = NULL;
	coloredNodes = NULL;

	worklistMoves = NULL;
	coalescedMoves = NULL;
	constrainedMoves = NULL;
	activeMoves = NULL;
	frozenMoves = NULL;
}

static void initLists(void) {
	moveList = checked_malloc(sizeof(*moveList) * Temp_count());
	degree = checked_malloc(sizeof(*degree) * Temp_count());
	adjList = checked_malloc(sizeof(*adjList) * Temp_count());
	alias = checked_malloc(sizeof(*alias) * Temp_count());
	color = checked_malloc(sizeof(*color) * Temp_count());
	int j;
	for(j = 0; j < Temp_count() - 1; j++) {
		moveList[j] = NULL;
		degree[j] = 0;
		adjList[j] = NULL;
		alias[j] = NULL;
	}
	Temp_tempList i;
	precolored = Temp_union(F_specialRegs(), Temp_union(F_calleeSaves(), F_callerSaves()));
	for(i = precolored; i; i = i->tail) {
		color[i->head->num] = i->head;
	}
}

static void Build(AS_instrList g) {
	AS_instrList i;
	for(i = g; i; i = i->tail) {
		AS_instr node = i->head;
		Temp_tempList temps;
		Temp_tempList live = Live_out(node);
		if(isMoveInstruction(node)) {
			live = Temp_union(live, FG_use(node));
			for(temps = Temp_union(FG_def(node), FG_use(node)); temps; temps = temps->tail) {
				Temp_temp temp = temps->head;
				moveList[temp->num] = AS_union(moveList[temp->num], A(node));
			}
			worklistMoves = AS_union(worklistMoves, A(node));
		}
		for(temps = FG_def(node); temps; temps = temps->tail) {
			Temp_tempList l;
			for(l = live; l; l = l->tail)
				AddEdge(l->head, temps->head);
		}
		initial = Temp_union(initial, FG_def(node));
	}
	initial = Temp_minus(initial, precolored);
}

static void makeWorklist(void) {
	Temp_tempList i;
	for(i = initial; i; i = i->tail) {
		Temp_temp n = i->head;
		int d = degree[n->num];
		if(degree[n->num] >= K) {
			spillWorklist = Temp_union(spillWorklist, Temp_TempList(n, NULL));
		}
		else if(MoveRelated(n)) {
			freezeWorklist = Temp_union(freezeWorklist, Temp_TempList(n, NULL));
		}
		else {
			simplifyWorklist = Temp_union(simplifyWorklist, Temp_TempList(n, NULL));
		}
	}
}

static void EnableMoves(Temp_tempList nodes) {
	Temp_tempList n;
	for(n = nodes; n; n = n->tail) {
		AS_instrList m;
		for(m = NodeMoves(n->head); m; m = m->tail) {
			if(AS_in(activeMoves, m->head)) {
				activeMoves = AS_minus(activeMoves, A(m->head));
				worklistMoves = AS_minus(worklistMoves, A(m->head));
			}
		}
	}
}

static void DecrementDegree(Temp_temp m) {
	int d = degree[m->num];
	degree[m->num] = d - 1;
	if(d == K) {
		EnableMoves(Temp_union(T(m), Adjacent(m)));
		spillWorklist = Temp_minus(spillWorklist, T(m));
		if(MoveRelated(m))
			freezeWorklist = Temp_union(freezeWorklist, T(m));
		else
			simplifyWorklist = Temp_union(simplifyWorklist, T(m));
	}
}

static void Simplify(void) {
	Temp_temp n = simplifyWorklist->head;
	simplifyWorklist = Temp_minus(simplifyWorklist, T(n));
	if(Temp_in(precolored, n))
		return;
	stackPush(n);
	//removeGraph(n);
	Temp_tempList m;
	for(m = Adjacent(n); m; m = m->tail) {
		DecrementDegree(m->head);
	}
}

static Temp_temp GetAlias(Temp_temp n) {
	if(Temp_in(coalescedNodes, n))
		return GetAlias(alias[n->num]);
	else
		return n;
}

static void AddWorklist(Temp_temp u) {
	if(!Temp_in(precolored, u) && !MoveRelated(u) && (degree[u->num] < K)) {
		freezeWorklist = Temp_union(freezeWorklist, T(u));
		simplifyWorklist = Temp_union(simplifyWorklist, T(u));
	}
}

static bool inAdjSet(Temp_temp u, Temp_temp v) {
	return Temp_in(adjList[u->num], v);
}
static bool OK(Temp_tempList l, Temp_temp u) {
	Temp_tempList i;
	bool ret = TRUE;
	for(i = l; i; i = i->tail) {
		Temp_temp t = i->head;
		ret = ret && ((degree[t->num] < K) || Temp_in(precolored, t) || inAdjSet(t, u));
	}
	return ret;
}

static int Conservative(Temp_tempList nodes) {
	int k = 0;
	Temp_tempList n;
	for(n = nodes; n; n = n->tail) {
		if(degree[n->head->num] >= K)
			k++;
	}
	return (k < K);
}

static void Combine(Temp_temp u, Temp_temp v) {
	if(Temp_in(freezeWorklist, v))
		freezeWorklist = Temp_minus(freezeWorklist, T(v));
	else
		spillWorklist = Temp_minus(spillWorklist, T(v));
	coalescedNodes = Temp_union(coalescedNodes, T(v));
	alias[v->num] = u;
	moveList[u->num] = AS_union(moveList[u->num], moveList[v->num]);
	EnableMoves(T(v));
	Temp_tempList t;
	for(t = Adjacent(v); t; t = t->tail) {
		AddEdge(t->head, u);
		DecrementDegree(t->head);
	}
	if((degree[u->num] <= K) && (Temp_in(freezeWorklist, u))) {
		freezeWorklist = Temp_minus(freezeWorklist, T(u));
		spillWorklist = Temp_union(spillWorklist, T(u));
	}
}

static void Coalesce(void) {
	AS_instr m = worklistMoves->head;
	Temp_temp x = GetAlias(m->u.MOVE.src->head);
	Temp_temp y = GetAlias(m->u.MOVE.dst->head);
	Temp_temp u,v;
	if(Temp_in(precolored, y)) {
		u = y;
		v = x;
	}
	else {
		u = x;
		v = y;
	}
	worklistMoves = AS_minus(worklistMoves, A(m));
	if(u == v) {
		coalescedMoves = AS_union(coalescedMoves, A(m));
		AddWorklist(u);
	}
	else if(Temp_in(precolored, v) || Temp_in(adjList[u->num], v)) {
		constrainedMoves = AS_union(constrainedMoves, A(m));
		AddWorklist(u);
		AddWorklist(v);
	}
	else if((Temp_in(precolored, u) && OK(Adjacent(v), u)) ||
			 (!Temp_in(precolored, u) && Conservative(Temp_union(Adjacent(u), Adjacent(v))))) {
		coalescedMoves = AS_union(coalescedMoves, A(m));
		Combine(u, v);
		AddWorklist(v);
	}
	else {
		activeMoves = AS_union(activeMoves, A(m));
	}
}

static void FreezeMoves(Temp_temp u) {
	AS_instrList m;
	for(m = NodeMoves(u); m; m = m->tail) {
		Temp_temp x = GetAlias(m->head->u.MOVE.src->head);
		Temp_temp y = GetAlias(m->head->u.MOVE.dst->head);
		Temp_temp v;
		if(GetAlias(y) == GetAlias(u)) {
			v = GetAlias(x);
		}
		else {
			v = GetAlias(y);
		}
		activeMoves = AS_minus(activeMoves, A(m->head));
		frozenMoves = AS_union(frozenMoves, A(m->head));
		if(NodeMoves(v) == NULL && degree[v->num] < K) {
			freezeWorklist = Temp_minus(freezeWorklist, T(v));
			simplifyWorklist = Temp_union(simplifyWorklist, T(v));
		}
	}
}

static void Freeze(void) {
	Temp_temp n = freezeWorklist->head;
	freezeWorklist = Temp_minus(freezeWorklist, T(n));
	simplifyWorklist = Temp_union(simplifyWorklist, T(n));
	FreezeMoves(n);
}

static void SelectSpill(void) {
	Temp_temp m_use = spillWorklist->head;
	Temp_tempList n;
	//heurística de seleção pelo número de usos
	for(n = spillWorklist; n; n = n->tail) {
		if(degree[n->head->num] >= degree[m_use->num]) {
			m_use = n->head;
		}
	}
	spillWorklist = Temp_minus(spillWorklist, T(m_use));
	simplifyWorklist = Temp_union(simplifyWorklist, T(m_use));
	FreezeMoves(m_use);
}

static void AssignColors(void) {
	while (selectStack != NULL) {
		Temp_temp n = stackPop();
		Temp_tempList okColors = Temp_TempList(F_EAX,
				Temp_TempList(F_EBX,
				Temp_TempList(F_ECX,
				Temp_TempList(F_EDX,
				Temp_TempList(F_ESI,
				Temp_TempList(F_EDI, NULL))))));
		Temp_tempList w;
		for(w = adjList[n->num]; w; w = w->tail) {
			if(Temp_in(Temp_union(coloredNodes, precolored), GetAlias(w->head))) {
				okColors = Temp_minus(okColors, T(color[GetAlias(w->head)->num]));
			}
		}
		if(okColors == NULL)
				spilledNodes = Temp_union(spilledNodes, T(n));
		else {
			coloredNodes = Temp_union(coloredNodes, T(n));
			Temp_temp c = okColors->head;
			color[n->num] = c;
			//printf("%s->%s\n", Temp_look(Temp_name(), n), Temp_look(Temp_name(), c));
		}
	}
	Temp_tempList m;
	for(m = coalescedNodes; m; m = m->tail) {
		Temp_temp n = m->head;
		Temp_temp o = GetAlias(n);
		color[n->num] = color[o->num];
		//printf("C %s->%s\n", Temp_look(Temp_name(), n), Temp_look(Temp_name(), color[o->num]));
	}
}

static Temp_tempList getSrc(AS_instr i) {
	switch(i->kind) {
		case I_MOVE:
			return i->u.MOVE.src;
			break;
		case I_OPER:
			return i->u.OPER.src;
			break;
	}
	return NULL;
}

static void Temp_replace(Temp_tempList temps, Temp_temp from, Temp_temp to) {
	Temp_tempList i;
	for(i = temps; i; i = i->tail) {
		if(i->head == from)
			i->head = to;
	}
}

static void RewriteProgram(F_frame f, AS_instrList il) {
	Temp_tempList i;
	Temp_tempList newTemps = NULL;
	char buf[128];
	for(i = spilledNodes; i; i = i->tail) {
		Temp_temp v = i->head;
		AS_instrList j;
		F_access a = F_allocLocal(f, TRUE);
		for(j = il; j; j = j->tail) {
			AS_instr instr = j->head;
			if(AS_in(coalescedMoves, instr)) {
				il = AS_minus(il, A(instr));
			}
			if(Temp_in(FG_def(instr), v)) {
				sprintf(buf, "movl\t%%`s0, %i(%%`d0) \t#spilled store\n", a->u.offset);
				Temp_temp new = Temp_newtemp();
				newTemps = Temp_union(newTemps, T(new));
				AS_instr new_store = AS_Oper(buf, T(F_FP), T(new), NULL);
				j->tail = AS_InstrList(new_store, j->tail);
				if(j->head->kind == I_MOVE)
					Temp_replace(j->head->u.MOVE.dst, v, new);
				else
					Temp_replace(j->head->u.OPER.dst, v, new);
			}
			if(Temp_in(FG_use(instr), v)) {
				sprintf(buf, "movl\t%i(%%`s0), %%`d0 \t#spilled fetch\n", a->u.offset);
				Temp_temp new = Temp_newtemp();
				newTemps = Temp_union(newTemps, T(new));
				AS_instr new_fetch = AS_Oper(buf, T(new), T(F_FP), NULL);
				j->tail = AS_InstrList(j->head, j->tail);
				j->head = new_fetch;
				j = j->tail;
				if(j->head->kind == I_MOVE)
					Temp_replace(j->head->u.MOVE.src, v, new);
				else
					Temp_replace(j->head->u.OPER.src, v, new);
			}
		}
	}
	spilledNodes = NULL;
	initial = Temp_union(coloredNodes, Temp_union(coalescedNodes, newTemps));
	coloredNodes = NULL;
	coalescedNodes = NULL;
}

void RA_Alloc(AS_instrList il) {
	AS_instrList i;
	for(i = il; i; i = i->tail) {
		AS_instr instr = i->head;
		switch(instr->kind) {
			case I_OPER: {
				Temp_tempList src;
				for(src = instr->u.OPER.src; src; src = src->tail) {
					if(!Temp_in(precolored, src->head))
						src->head = color[src->head->num];
				}
				Temp_tempList dst;
				for(dst = instr->u.OPER.dst; dst; dst = dst->tail) {
					if(!Temp_in(precolored, dst->head))
						dst->head = color[dst->head->num];
				}
				break;
			}
			case I_MOVE: {
				Temp_tempList src;
				for(src = instr->u.MOVE.src; src; src = src->tail) {
					if(!Temp_in(precolored, src->head))
						src->head = color[src->head->num];
				}
				Temp_tempList dst;
				for(dst = instr->u.MOVE.dst; dst; dst = dst->tail) {
					if(!Temp_in(precolored, dst->head))
						dst->head = color[dst->head->num];
				}
				break;
			}
		}
	}
}

void printCoalesces(Temp_tempList list) {
	if(list == NULL)
		return;
	else {
		printf("%i-%i", list->head->num, GetAlias(list->head)->num);
		if(list->tail)
			printf(", ");
		printCoalesces(list->tail);
	}
}

static void Debug(string name) {
	printf("%s: \n", name);
	printf("coalesced nodes: %i\n", Temp_listCount(coalescedNodes));
	printf("spilled: %i\n", Temp_listCount(spilledNodes));
}

void RA_regAlloc(F_frame f, AS_instrList il) {
	initLists();
	Live_liveness(FG_AssemFlowGraph(il));
	Build(il);
	makeWorklist();
	//printGraph();
	do {
		//Debug();
		if(simplifyWorklist != NULL)
			Simplify();
		else if(worklistMoves != NULL)
			Coalesce();
		else if(freezeWorklist != NULL)
			Freeze();
		else if(spillWorklist != NULL)
			SelectSpill();
	}
	while((spillWorklist != NULL) ||
			(worklistMoves != NULL) ||
			(simplifyWorklist != NULL) ||
			(freezeWorklist != NULL));
	AssignColors();
#ifdef DEBUG
	Debug(S_name(F_name(f)));
#endif
	if(spilledNodes != NULL) {
		int d = Temp_listCount(spilledNodes);
		RewriteProgram(f, il);
		RA_regAlloc(f, il);
	}
}

