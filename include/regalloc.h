#ifndef H_REGALLOC_H
#define H_REGALLOC_H
/* function prototype from regalloc.c */
void RA_initUses(void);
void RA_Alloc(AS_instrList il);
void RA_regAlloc(F_frame f, AS_instrList il);
#endif
