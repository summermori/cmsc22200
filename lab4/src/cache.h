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

} cache_t;

typedef struct
{

} block_t;

extern cache_t INST_CACHE;
extern cache_t DATA_CACHE;

// IGNORE THESE FOR NOW

// void cache_destroy(cache_t *c); // dealocates
// int cache_update(cache_t *c, uint64_t addr);

cache_t cache_new(int sets, int ways, int block); // allocates

uint_32 cache_read(uint_32 addr, uint_32int n);

void cache_write (uint_32 addr, uint_32 val);

#endif
