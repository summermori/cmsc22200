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
} IFtoID_t;

typedef struct IDtoEX_t {
	int64_t op;
	int64_t m;
	int64_t n;
	int64_t d; // this will probably remain as a 5-bit number
	int64_t imm1;
	int64_t imm2;
	int64_t addr;
} IDtoEX_t;

typedef struct EXtoMEM_t {
	int64_t op;
	int64_t n;
	int64_t d; // this will probably remain as a 5-bit number
	int64_t addr;
	int64_t res;
	int fn; // this, along with other data must be propagated down the pipeline
	int fz;
} EXtoMEM_t;

typedef struct MEMtoWB_t {
	int64_t d;
	int64_t res;
	int fn;
	int fz;
} MEMtoWB_t;

/* control struct */
// we will need a control struct for stalling and fowarding signals

#endif
