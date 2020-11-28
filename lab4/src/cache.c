/*
 * CMSC 22200, Fall 2016
 *
 * ARM pipeline timing simulator
 *
 */

#include "cache.h"
#include "shell.h"
#include "bp.h"
#include <stdlib.h>
#include <stdio.h>

uint32_t cache_read(uint64_t addr, int n) //cache_read takes the read location and the associativity, and returns a uint64_t from either memory or the cache
{
  block_t *cache;
  uint64_t tag;
  uint64_t index;
  if (n == 4) { //condition to establish we are in the instruction cache
    tag = (addr & 0xfffff800) >> 11; //the tag used for matching instruction blocks
    index = (addr & 0x000007e0) >> 5;		//the specific set for the instruction
    cache = INST_CACHE;
  }
  else {
    tag = (addr & 0xffffe000) >> 13;		//the tag used for matching data blocks
    index = (addr & 0x00001fe0) >> 5;		//the specific set for the data
    cache = DATA_CACHE;
  }

  uint64_t offset = addr & 0x1f;		//the specific segment of the matching block

  uint64_t head = (index * n);
  uint64_t tail = (head + n);

  //declaring variables ahead of time to track the lru mins during the for loop
  uint64_t min_i = 0;
  uint64_t min_val = INT_MAX;

  int empty = -1;

  //go through each of the blocks of the set. If you find a match, update the lru and return
  for (int i = head; i < tail; i++) {
    block_t spec_block = cache[i]; // NOTE: figure out the relationship between block_t and cache_t

    //prepare data for loading into an empty block or eviction
    if (spec_block.empty) {
      empty = i;
    }
    else if (spec_block.lru < min_val) {
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
  if (n == 4 && check_branch_ahead(addr) == 1)
  {
	return 0;
  }

  //we are now guaranteed to be doing a read, and so we can signal the stall dependent on the type of miss
  if (n == 4)
  {
    Control.inst_cache_bubble = 51;
  }
  else
  {
    Control.data_cache_bubble = 51;
  }


  //check if there's an empty block. If so, we load into there
  if (empty != -1) {
    block_t new_block = cache[empty];

    //load in all the variables
    new_block.tag = tag;
    new_block.valid = 1;
    new_block.lru = stat_cycles;

    //calculate the "offset-less" addr value, then grab all valid blocks with that head - see note
    uint64_t addr_head = addr & 0xffffffe0;
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
      uint64_t write_base = (tag << 13) ^ (index << 5);
      for (int i = 0; i < 8; i++) {
        mem_write_32((write_base + (i * 4)), lru_block.data[i]);
      }
      lru_block.dirty = 0;
    }

    lru_block.tag = tag;
    lru_block.lru = stat_cycles;

    uint64_t addr_head = addr & 0xffffffe0;
    for (int i = 0; i < 8; i++) {
      lru_block.data[i] = mem_read_32(addr_head + (4 * i));
    }

    //return the newly-read address
    return lru_block.data[offset];
  }
}

void cache_write (uint64_t addr, uint64_t val) {

  uint64_t tag = (addr & 0xffffe000) >> 13;		//the tag used for matching data blocks
  uint64_t index = (addr & 0x00001fe0) >> 5;		//the specific set for the data
  uint64_t offset = addr & 0x1f;		//the specific segment of the matching block
  block_t *cache = DATA_CACHE;

  uint64_t head = (index * 8);
  uint64_t tail = (head + 8);

  //declaring variables ahead of time to track the lru mins during the for loop
  uint64_t min_i = 0;
  uint64_t min_val = INT_MAX;

  int empty = -1;

  //go through each of the blocks of the set. If you find a match, update the lru and return
  for (int i = head; i < tail; i++) {
    block_t spec_block = cache[i];

    //prepare data for loading into an empty block or eviction
    if (spec_block.empty) {
      empty = i;
    }
    else if (spec_block.lru < min_val) {
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
  Control.data_cache_bubble = 51;

  //check if there's an empty block. If so, we load into there
  if (empty != -1) {
    block_t new_block = cache[empty];

    //load in all the variables
    new_block.tag = tag;
    new_block.valid = 1;
    new_block.lru = stat_cycles;

    //calculate the "offset-less" addr value, then grab all valid blocks with that head - see note
    uint64_t addr_head = addr & 0xffffffe0;
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
      uint64_t write_base = (tag << 13) ^ (index << 5);
      for (int i = 0; i < 8; i++) {
        mem_write_32((write_base + (i * 4)), lru_block.data[i]);
      }
    }

    lru_block.tag = tag;
    lru_block.lru = stat_cycles;

    uint64_t addr_head = addr & 0xffffffe0;
    for (int i = 0; i < 8; i++) {
      lru_block.data[i] = mem_read_32(addr_head + (4 * i));
    }

    //write in the value to the correct segment
    lru_block.dirty = 1;
    lru_block.data[offset] = val;
  }
}


int check_branch_ahead(uint64_t addr)
{
    uint64_t base = CURRENT_STATE.PC - 4;
    int64_t offset;

    //cond
    if (IDtoEX.op == 0x54000000)
    {
        int64_t cond = IDtoEX.dnum;
        offset = IDtoEX.addr >> 5;
        //check if it is branching
        switch(cond)
        {
            //BEQ
            case(0):
                if (Control.fz == 1)
                {
                    if ((base + offset) == addr)
                    {
                        return 0;
                    }
                    else
                    {
                        return 1;
                    }
                }
                else
                {
                    return 0;
                }
            //BNE
            case(1):
                if (Control.fz == 0)
                {
                    if ((base + offset) == addr)
                    {
                        return 0;
                    }
                    else
                    {
                        return 1;
                    }
                }
                else
                {
                    return 0;
                }
            //BGE
            case(10):
                if ((Control.fz == 1) || (Control.fn == 0))
                {
                    if ((base + offset) == addr)
                    {
                        return 0;
                    }
                    else
                    {
                        return 1;
                    }
                }
                else
                {
                    return 0;
                }
            //BLT
            case(11):
                if ((Control.fn == 1) || (Control.fz == 0))
                {
                    if ((base + offset) == addr)
                    {
                        return 0;
                    }
                    else
                    {
                        return 1;
                    }
                }
                else
                {
                    return 0;
                }
            //BGT
            case(12):
                if ((Control.fz == 0) || (Control.fn == 0))
                {
                    if ((base + offset) == addr)
                    {
                        return 0;
                    }
                    else
                    {
                        return 1;
                    }
                }
                else
                {
                    return 0;
                }
            //BLE
            case(13):
                if ((Control.fz == 1) || (Control.fn == 1))
                {
                    if ((base + offset) == addr)
                    {
                        return 0;
                    }
                    else
                    {
                        return 1;
                    }
                }
                else
                {
                    return 0;
                }
        }

    }
    //uncond reg
    if (IDtoEX.op == 0xd61f0000)
    {
        int64_t target;
        if (EXtoMEM.dnum == IDtoEX.n)
        {
            target = EXtoMEM.res;
        }
        else
        {
            target = CURRENT_STATE.REGS[IDtoEX.n];
        }

        if (target == addr)
        {
            return 0;
        }
        else
        {
            return 1;
        }
    }

    //uncond imm
    if (IDtoEX.op == 0x14000000)
    {
        offset = IDtoEX.addr;
        if ((base + offset) == addr)
        {
            return 0;
        }
        else
        {
            return 1;
        }
    }

    //CBZ
    if (IDtoEX.op == 0xb4000000)
    {
        offset = IDtoEX.addr/32;
        int reg_load_ahead = 0;
        switch(EXtoMEM.op)
        {
            // Add/Subtract immediate
            case 0x91000000:
            //printf("ADD\n");
            reg_load_ahead = 1;
            break;
            case 0xb1000000:
            //printf("ADDS\n");
            reg_load_ahead = 1;
            break;
            case 0xd1000000:
            // printf("SUB\n");
            reg_load_ahead = 1;
            break;
            case 0xf1000000:
            // printf("SUBS\n");
            reg_load_ahead = 1;
            break;
            // Compare and branch
            case 0xb4000000:
            //printf("CBZ\n");
            break;
            case 0xb5000000:
            //printf("CBNZ\n");
            break;
            // Move wide
            case 0xd2800000:
            //printf("MOVZ\n");
            reg_load_ahead = 1;
            break;
            // Bitfield
            case 0xd3000000:
            //printf("LSL or LSR\n"); //execution has to do the distinction
            break;
            // Conditional branch
            case 0x54000000:
            //printf("B.cond\n");
            break;
            // Exceptions
            case 0xd4400000:
            //printf("HLT\n");
            break;
            // Unconditional branch (register)
            case 0xd61f0000:
            //printf("BR\n");
            break;
            // Unconditional branch (immediate)
            case 0x14000000:
            //printf("B\n");
            break;

            // Logical (shifted register)
            case 0x8a000000:
            //printf("AND\n");
            reg_load_ahead = 1;
            break;
            case 0xea000000:
            //printf("ANDS\n");
            reg_load_ahead = 1;
            break;
            case 0xca000000:
            //printf("EOR\n");
            reg_load_ahead = 1;
            break;
            case 0xaa000000:
            //printf("ORR\n");
            reg_load_ahead = 1;
            break;
            // Add/subtract (extended)
            case 0x8b000000:
            //printf("ADD\n");
            reg_load_ahead = 1;
            break;
            case 0xab000000:
            //printf("ADDS\n");
            reg_load_ahead = 1;
            break;
            case 0xcb000000:
            //printf("SUB\n");
            reg_load_ahead = 1;
            break;
            case 0xeb000000:
            //printf("SUBS\n");
            reg_load_ahead = 1;
            break;
            // Data Processing (3 source)
            case 0x9b000000:
            //printf("MUL\n");
            reg_load_ahead = 1;
            break;
        }
        if (((EXtoMEM.dnum == IDtoEX.dnum) && (EXtoMEM.res == 0) && (reg_load_ahead == 1)))
        {
            if ((base + offset) == addr)
            {
                return 0;
            }
            else
            {
                return 1;
            }
        }
        else if (((EXtoMEM.dnum == IDtoEX.dnum) && (EXtoMEM.res != 0) && (reg_load_ahead == 1)))
        {
            return 0;
        }
        else if (CURRENT_STATE.REGS[IDtoEX.dnum] == 0)
        {
            if ((base + offset) == addr)
            {
                return 0;
            }
            else
            {
                return 1;
            }
        }
        else
        {
            return 0;
        }


    }
    //CBNZ
    if (IDtoEX.op == 0xb5000000)
    {
        offset = IDtoEX.addr/32;
        int reg_load_ahead = 0;
        switch(EXtoMEM.op)
        {
            // Add/Subtract immediate
            case 0x91000000:
            //printf("ADD\n");
            reg_load_ahead = 1;
            break;
            case 0xb1000000:
            //printf("ADDS\n");
            reg_load_ahead = 1;
            break;
            case 0xd1000000:
            // printf("SUB\n");
            reg_load_ahead = 1;
            break;
            case 0xf1000000:
            // printf("SUBS\n");
            reg_load_ahead = 1;
            break;
            // Compare and branch
            case 0xb4000000:
            //printf("CBZ\n");
            break;
            case 0xb5000000:
            //printf("CBNZ\n");
            break;
            // Move wide
            case 0xd2800000:
            //printf("MOVZ\n");
            reg_load_ahead = 1;
            break;
            // Bitfield
            case 0xd3000000:
            //printf("LSL or LSR\n"); //execution has to do the distinction
            break;
            // Conditional branch
            case 0x54000000:
            //printf("B.cond\n");
            break;
            // Exceptions
            case 0xd4400000:
            //printf("HLT\n");
            break;
            // Unconditional branch (register)
            case 0xd61f0000:
            //printf("BR\n");
            break;
            // Unconditional branch (immediate)
            case 0x14000000:
            //printf("B\n");
            break;

            // Logical (shifted register)
            case 0x8a000000:
            //printf("AND\n");
            reg_load_ahead = 1;
            break;
            case 0xea000000:
            //printf("ANDS\n");
            reg_load_ahead = 1;
            break;
            case 0xca000000:
            //printf("EOR\n");
            reg_load_ahead = 1;
            break;
            case 0xaa000000:
            //printf("ORR\n");
            reg_load_ahead = 1;
            break;
            // Add/subtract (extended)
            case 0x8b000000:
            //printf("ADD\n");
            reg_load_ahead = 1;
            break;
            case 0xab000000:
            //printf("ADDS\n");
            reg_load_ahead = 1;
            break;
            case 0xcb000000:
            //printf("SUB\n");
            reg_load_ahead = 1;
            break;
            case 0xeb000000:
            //printf("SUBS\n");
            reg_load_ahead = 1;
            break;
            // Data Processing (3 source)
            case 0x9b000000:
            //printf("MUL\n");
            reg_load_ahead = 1;
            break;
        }
        if (((EXtoMEM.dnum == IDtoEX.dnum) && (EXtoMEM.res != 0) && (reg_load_ahead == 1)))
        {
            if ((base + offset) == addr)
            {
                return 0;
            }
            else
            {
                return 1;
            }
        }
        else if (((EXtoMEM.dnum == IDtoEX.dnum) && (EXtoMEM.res == 0) && (reg_load_ahead == 1)))
        {
            return 0;
        }
        else if (CURRENT_STATE.REGS[IDtoEX.dnum] == 0)
        {
            if ((base + offset) == addr)
            {
                return 0;
            }
            else
            {
                return 1;
            }
        }
        else
        {
            return 0;
        }
    }
}
