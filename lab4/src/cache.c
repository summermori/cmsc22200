/*
 * CMSC 22200, Fall 2016
 *
 * ARM pipeline timing simulator
 *
 */

#include "cache.h"
#include <stdlib.h>
#include <stdio.h>

// IGNORE THESE FOR NOW
// void cache_destroy(cache_t *c)
// {}
// int cache_update(cache_t *c, uint64_t addr)
// {}
// cache_t *cache_new(int sets, int ways, int block)
// {}

uint_32 cache_read(uint_32 addr, int n) //cache_read takes the read location and the associativity, and returns a uint_32 from either memory or the cache
{
  if (n == 4) { //condition to establish we are in the instruction cache
    uint_32 tag = (addr & 0xfffff800) >> 11; //the tag used for matching instruction blocks
    uint_32 index = (addr & 0x000007e0) >> 5;		//the specific set for the instruction
    cache_t cache = INST_CACHE;
  }
  else {
    uint_32 tag = (addr & 0xffffe000) >> 13;		//the tag used for matching data blocks
    uint_32 index = (addr & 0x00001fe0) >> 5;		//the specific set for the data
    cache_t cache = DATA_CACHE;
  }

  uint_32 offset = addr & 0x1f;		//the specific segment of the matching block

  uint_32 head = (index * n);
  uint_32 tail = (head + n);

  //declaring variables ahead of time to track the lru mins during the for loop
  uint_32 min_i = 0;
  uint_32 min_val = MAX_INT;

  int empty = -1;

  //go through each of the blocks of the set. If you find a match, update the lru and return
  for (int i = head; i < tail; i++) {
    block_t spec_block = cache[i]; // NOTE: figure out the relationship between block_t and cache_t

    //prepare data for loading into an empty block or eviction
    if (spec_block.empty) {
      empty = i;
    }
    elif (spec_block.lru < min_val) {
      min_i = i;
      min_val = spec_block.lru;
    }

    if (spec_block.valid && spec_block.tag == tag) {
      spec_block.lru = stat_cycles;
      return spec_block.data[offset];
    }
  }

  //a miss has occured - different steps here for different caches:

  //if this is the instruction cache AND there is an upcoming branch AND that branch is not to this addr:
  if ((n == 4) && (IDtoEX.branching) && ((IDtoEX)!= addr)) {
    //return garbage? It shouldn't matter what we return here, it'll never go through after flush
    return 0;
  }

  //we are now guaranteed to be doing a read, and so we can signal the stall
  //{stall_signal for 50 cycles}

  //check if there's an empty block. If so, we load into there
  if (empty != -1) {
    block_t new_block = cache[empty];

    //load in all the variables
    new_block.tag = tag;
    new_block.valid = 1;
    new_block.lru = stat_cycles;

    //calculate the "offset-less" addr value, then grab all valid blocks with that head - see note
    uint_32 addr_head = addr & 0xffffffe0;
    for (int i = 0; i < 8; i++) {
      new_block.data[i] = mem_read_32(addr_head + (4 * i));
    }

    //return the newly-read address
    return new_block.data[offset];
  }
  //if there isn't, we must evict, which is handled differently if the dirty bit is 1 AND n == 8
  else {
    block_t lru_block = cache[min_i];
    if (lru_block.dirty && (n == 8)) {
      uint_32 write_base = (tag << 13) ^ (index << 5);
      for (int i = 0; i < 8; i++) {
        mem_write_32((write_base + (i * 4)), lru_block.data[i]);
      }
      lru_block.dirty = 0;
    }

    lru_block.tag = tag;
    lru_block.lru = stat_cycles;

    uint_32 addr_head = addr & 0xffffffe0;
    for (int i = 0; i < 8; i++) {
      lru_block.data[i] = mem_read_32(addr_head + (4 * i));
    }

    //return the newly-read address
    return lru_block.data[offset];
  }
}

void cache_write (uint_32 addr, uint_32 val) {

  uint_32 tag = (addr & 0xffffe000) >> 13;		//the tag used for matching data blocks
  uint_32 index = (addr & 0x00001fe0) >> 5;		//the specific set for the data
  uint_32 offset = addr & 0x1f;		//the specific segment of the matching block
  cache_t cache = DATA_CACHE;

  uint_32 head = (index * n);
  uint_32 tail = (head + n);

  //declaring variables ahead of time to track the lru mins during the for loop
  uint_32 min_i = 0;
  uint_32 min_val = MAX_INT;

  int empty = -1;

  //go through each of the blocks of the set. If you find a match, update the lru and return
  for (int i = head; i < tail; i++) {
    bloack_t spec_block = cache[i];

    //prepare data for loading into an empty block or eviction
    if (spec_block.empty) {
      empty = i;
    }
    elif (spec_block.lru < min_val) {
      min_i = i;
      min_val = spec_block.lru;
    }

    if (spec_block.valid && spec_block.tag == tag) {
      spec_block.lru = stat_cycles;
      spec_block.dirty = 1;
      spec_block.data[offset] = val;
    }
  }

  //a miss has occured, and as such we have to read in the correct block.
  //{stall_signal for 50 cycles}

  //check if there's an empty block. If so, we load into there
  if (empty != -1) {
    block_t new_block = cache[empty]

    //load in all the variables
    new_block.tag = tag;
    new_block.valid = 1;
    new_block.lru = stat_cycles;

    //calculate the "offset-less" addr value, then grab all valid blocks with that head - see note
    uint_32 addr_head = addr & 0xffffffe0;
    for (int i = 0; i < 8; i++) {
      new_block.data[i] = mem_read_32(addr_head + (4 * i));
    }

    //write in the value to the correct segment
    new_block.data[offset] = val;
  }
  //if there isn't, we must evict, which is handled differently if the dirty bit is 1
  else {
    block_t lru_block = cache[min_i];
    if (lru_block.dirty) {
      write_base = (tag << 13) ^ (index << 5);
      for (int i = 0; i < 8; i++) {
        mem_write_32((write_base + (i * 4)), lru_block.data[i]);
      }
    }

    lru_block.tag = tag;
    lru_block.lru = stat_cycles;

    uint_32 addr_head = addr & 0xffffffe0;
    for (int i = 0; i < 8; i++) {
      lru_block.data[i] = mem_read_32(addr_head + (4 * i));
    }

    //write in the value to the correct segment
    lru_block.dirty = 1;
    lru_block.data[offset] = val;
  }
}
