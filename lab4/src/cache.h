/*
 * CMSC 22200, Fall 2016
 *
 * ARM pipeline timing simulator
 *
 */
#ifndef _CACHE_H_
#define _CACHE_H_

#include <stdint.h>

typedef struct
{
  uint64_t tag;
  unsigned char valid;
  unsigned char dirty;
  int empty;
  uint32_t lru;
  uint32_t data[8];
} block_t;

//typedef block_t[] cache_t;

extern block_t INST_CACHE[];
extern block_t DATA_CACHE[];

// IGNORE THESE FOR NOW

// void cache_destroy(cache_t *c); // dealocates
// int cache_update(cache_t *c, uint64_t addr);
//cache_t cache_new(int sets, int ways, int block); // allocates

uint32_t cache_read(uint64_t addr, int n);

void cache_write (uint64_t addr, uint64_t val);

int check_branch_ahead(uint64_t addr);

#endif
