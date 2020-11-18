/*
 * CMSC 22200
 *
 * ARM pipeline timing simulator
 */

#ifndef _PIPE_H_
#define _PIPE_H_

#include "shell.h"
#include "stdbool.h"
#include <limits.h>


typedef struct CPU_State {
	/* register file state */
	int64_t REGS[ARM_REGS];
	int FLAG_N;        /* flag N */
	int FLAG_Z;        /* flag Z */

	/* program counter in fetch stage */
	uint64_t PC;

} CPU_State;

int RUN_BIT;

/* global variable -- pipeline state */
extern CPU_State CURRENT_STATE;

/* called during simulator startup */
void pipe_init();

/* this function calls the others */
void pipe_cycle();

/* each of these functions implements one stage of the pipeline */
void pipe_stage_fetch();
void pipe_stage_decode();
void pipe_stage_execute();
void pipe_stage_mem();
void pipe_stage_wb();

/* pipeline registers */
typedef struct IFtoID_t {
	uint32_t inst;
	int pc_halt;
} IFtoID_t;

typedef struct IDtoEX_t {
	int64_t op;
	int64_t m;
	int64_t n;
	int64_t dnum;
	int64_t dval;
	int64_t imm1;
	int64_t imm2;
	int64_t addr;
	int fmem;
	int fwb;
	int branching;
} IDtoEX_t;

typedef struct EXtoMEM_t {
	int64_t op;
	int64_t n;
	int64_t dnum;
	int64_t dval;
	int64_t imm1;
	int64_t res;
	int fmem;
	int fwb;
	int fn; // this, along with other data must be propagated down the pipeline
	int fz;
	int halt;
	int branching;
} EXtoMEM_t;

typedef struct MEMtoWB_t {
	int64_t op;
	int64_t dnum;
	int64_t res;
	int fwb;
	int fn;
	int fz;
	int halt;
	int branching;
	int fmem;
} MEMtoWB_t;

/*structs for the queue */
typedef struct Prediction {
	int prediction_taken;
	uint32_t pc_before_prediction;
	uint64_t taken_target;
}Prediction;

typedef struct entry { 
    struct Prediction pred; 
}entry; 
  
typedef struct queue { 
    int maxlen;
    int currlen;
    struct entry *head;
    struct entry *tail; 
}queue; 

/* control struct */
// we will need a control struct for stalling and fowarding signals
typedef struct Control_t {
	//lab3 stuff
	int pre_halt;
	int lab3_bubble;
	//lab2 branch bubble stuff
	int compare_branch;
	int cond_branch;
	uint32_t branch_grab;
	uint32_t squashed;
	int not_taken;
	int branch_bubble_until;
	//lab2 loadstore bubble stuff
	int loadstore_bubble_until;
	int loadstore_bubble_start;
	int restoration;
	//halt, flags, and baddr
	int halt;
	int baddr;
	int bpc;
	int fn;
	int fz;
} Control_t;

extern Control_t Control;
extern queue q;

/*Queue Functions*/
struct entry* newentry(struct Prediction pred);
struct entry* dequeue(struct queue* q);
void decaptiate(struct queue* q);
struct queue* createqueue(int maxlen);
void enqueue(struct queue* q, struct Prediction pred);
void freequeue(struct queue* q);

/* Bubble Functions */
void TriggerBubble_Branch(int bubble_until);
void TriggerBubble_LoadStore(int bubble_until);
void condBubble(int64_t cond);
/* instruction helpers */
void Branch(int64_t offset);
/* lab3 restore and flush helper*/
void Restore_Flush(uint32_t real_target, int pred_taken, int branch_taken, uint32_t pc_before_prediction, uint64_t taken_target);
/* instruction implementations */
void CBNZ();
void CBZ();
void MUL();
void HLT();
void BR();
void B();
void B_Cond();
void LDUR();
void LDUR2();
void LDURH();
void LDURB();
void STUR();
void STUR2();
void STURH();
void STURB();
void ADDS_Immediate();
void ADD_Immediate();
void ADDS_Extended();
void ADD_Extended();
void AND();
void ANDS();
void EOR();
void ORR();
void BITSHIFT();
void MOVZ();
void SUB_Immediate();
void SUB_Extended();
void SUBS_Immediate();
void SUBS_Extended();

#endif
