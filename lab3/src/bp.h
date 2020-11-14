/*
 * ARM pipeline timing simulator
 *
 * CMSC 22200, Fall 2016
 */

#ifndef _BP_H_
#define _BP_H_

#include "shell.h"
#include "pipe.h"

typedef struct BTB_Entry {
	uint64_t addr_tag;
	unsigned char valid_bit;
	unsigned char	cond_bit;
	uint64_t target;
} btb_entry_t;

typedef struct
{
  unsigned char gshare;
  unsigned char pht[64];
  btb_entry_t btb_table[1024];
} bp_t;



void bp_predict(uint64_t fetch_pc);
void bp_update(uint64_t fetch_pc, unsigned char cond_bit, uint64_t target, int inc);

#endif
