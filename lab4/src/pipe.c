/*
 * CMSC 22200
 *
 * ARM pipeline timing simulator
 */

#include "pipe.h"
#include "shell.h"
#include "cache.h"
#include "bp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/queue.h>

/* helper funcs */
int isSturBranch(int word)
{
  if (((word & 0xfc000000) == 0x14000000) || ((word & 0xbfe00000) == 0xb8000000) || ((word & 0xbfe00000) == 0x38000000))
    return 1;
  else
    return 0;
}

/* global pipeline state */
CPU_State CURRENT_STATE;

void pipe_init()
{
    memset(&CURRENT_STATE, 0, sizeof(CPU_State));
    CURRENT_STATE.PC = 0x00400000;
}


IFtoID_t IFtoID = { .inst = 0};
IDtoEX_t IDtoEX = { .op = 0, .m = 0, .n = 0, .dnum = 0, .imm1 = 0, .imm2 = 0, .addr = 0, .fmem = 0, .fwb = 0};
EXtoMEM_t EXtoMEM = { .n = 0, .dnum = 0, .dval = 0, .imm1 = 0, .res = 0, .fmem = 0, .fwb = 0, .fn = 0, .fz = 0};
MEMtoWB_t MEMtoWB = {.dnum = 0, .res = 0, .fwb = 0, .fn = 0, .fz = 0};
//declaration in pipe.h so bp.c can access Control struct
Control_t Control = {.baddr= -1, .branch_bubble_until = -1, .branch_grab = 0, .loadstore_bubble_until = -1, .restoration = -1, .fn = 0, .fz = 0, .halt = 0};
EXtoMEM_t dcache_store_EXtoMEM;
IFtoID_t temp_IFtoID;
IDtoEX_t temp_IDtoEX;
int loadstore_dependency = 0;
queue q = {.maxlen = 2, .currlen = 0, .head = NULL, .tail = NULL};


int64_t SIGNEXTEND(int64_t offset) {
  if (offset & 0x0000000000000100) {
    offset = (offset | 0xffffffffffffff00);
  }
  return offset;
}

void compareBubble(int64_t reg_val)
{
  //only bubble on cbnz
  if ((EXtoMEM.dnum != reg_val) && (IDtoEX.op == 0xb5000000) && (CURRENT_STATE.REGS[reg_val] == 0))
  {
    TriggerBubble_Branch((int) stat_cycles + 2);
  }
  return;
}

void condBubble(int64_t cond) // a helper to determine when conditional branching should bubble
{
  switch(cond)
  {
      //BEQ
      case(0):
          if (Control.fz != 1)
          {TriggerBubble_Branch((int) stat_cycles + 2);}
          break;
      //BNE
      case(1):
          if (Control.fz != 0)
          {TriggerBubble_Branch((int) stat_cycles + 2);}
          break;
      //BGE
      case(10):
          if ((Control.fz != 1) && (Control.fn != 0))
          {TriggerBubble_Branch((int) stat_cycles + 2);}
          break;
      //BLT
      case(11):
          if ((Control.fn != 1) || (Control.fz != 0))
          {TriggerBubble_Branch((int) stat_cycles + 2);}
          break;
      //BGT
      case(12):
          if ((Control.fn != 0) || (Control.fz != 0))
          {TriggerBubble_Branch((int) stat_cycles + 2);}
          break;
      //BLE
      case(13):
          if ((Control.fz != 1) && (Control.fn != 1))
          {TriggerBubble_Branch((int) stat_cycles + 2);}
          break;
    }
}

int64_t reg_call(int64_t addr) {
        //for the exe struct
        if (addr != 31) {
                if ((EXtoMEM.dnum == addr) && !(isSturBranch(addr)) && (EXtoMEM.fmem == 0)) {
                        return EXtoMEM.res;
                }
                else if ((MEMtoWB.dnum == addr) && !(isSturBranch(addr)) && (MEMtoWB.fwb == 1)) {
                        return MEMtoWB.res;
                }
        }
        return CURRENT_STATE.REGS[addr];
}

void pipe_cycle()
{
	pipe_stage_wb();
	pipe_stage_mem();
	pipe_stage_execute();
	pipe_stage_decode();
	pipe_stage_fetch();
}

void pipe_stage_wb()
{
  //WB stall on data_cache_bubble
  if (Control.data_cache_bubble > 0)
  {
    return;
  }

  //exception
  if (MEMtoWB.halt == 1)
  {
    RUN_BIT = false;
    stat_inst_retire = stat_inst_retire + 1;
  }
  else if ((MEMtoWB.dnum != 31) && (MEMtoWB.fwb)){
    CURRENT_STATE.REGS[MEMtoWB.dnum] = MEMtoWB.res;
  }
  CURRENT_STATE.FLAG_Z = MEMtoWB.fz;
  CURRENT_STATE.FLAG_N = MEMtoWB.fn;
  if (MEMtoWB.dnum != 0 || MEMtoWB.fwb != 0 || MEMtoWB.fmem != 0 || MEMtoWB.branching != 0)
  {
    stat_inst_retire = stat_inst_retire + 1;
  }
  MEMtoWB = (MEMtoWB_t){.dnum = 0, .fwb = 0, .res = 0, .fn = 0, .fz = 0, .branching = 0};
}

void pipe_stage_mem()
{
  //middle or last cycle of bubble
  if (Control.data_cache_bubble > 0)
  {
    //middle of cycle
    if (Control.data_cache_bubble > 1)
    {
      return;
    }
    //last cycle of bubble, data_cache_bubble == 1
    else if (Control.data_cache_bubble == 1)
    {
      //stall dismount, hit on the same inst we missed on
      int64_t t;
      int64_t n;
      int64_t offset;
      uint64_t load;
      uint64_t load2;
      switch (dcache_store_EXtoMEM.op)
      {
        case 0xf8400000:
          t = dcache_store_EXtoMEM.dnum;
          n = dcache_store_EXtoMEM.n;
          offset = SIGNEXTEND(dcache_store_EXtoMEM.imm1);
          load = cache_read(n + offset, 8);
          load2 = cache_read(n + offset + 4, 8);
          load = load | (load2 << 32);
          if (t != 31) {
            MEMtoWB.res = load;
          }
          MEMtoWB.fwb = 1;
          break;
        case 0xb8400000:
          t = dcache_store_EXtoMEM.dnum;
          n = dcache_store_EXtoMEM.n;
          offset = SIGNEXTEND(dcache_store_EXtoMEM.imm1);
          load = cache_read(n + offset, 8);
          if (t != 31) {
            MEMtoWB.res = load;
          }
          MEMtoWB.fwb = 1;
          break;
        case 0x38400000:
          t = dcache_store_EXtoMEM.dnum;
          n = dcache_store_EXtoMEM.n;
          offset = SIGNEXTEND(dcache_store_EXtoMEM.imm1);
          load = cache_read(n + offset, 8);
          load = (load & 0x000000ff);
          if (t != 31) {
            MEMtoWB.res = load;
          }
          MEMtoWB.fwb = 1;
          break;
        case 0x78400000:
          t = dcache_store_EXtoMEM.dnum;
          n = dcache_store_EXtoMEM.n;
          offset = SIGNEXTEND(dcache_store_EXtoMEM.imm1);
          load = cache_read(n + offset, 8);
          load = (load & 0x0000ffff);
          if (t != 31) {
            MEMtoWB.res = load;
          }
          MEMtoWB.fwb = 1;
          break;
        case 0xf8000000:
          t = dcache_store_EXtoMEM.dval;
          n = dcache_store_EXtoMEM.n;
          offset = SIGNEXTEND(dcache_store_EXtoMEM.imm1);
          cache_write(n + offset, (t & 0x00000000ffffffff));
          cache_write(n + offset + 4, ((t & 0xffffffff00000000) >> 32));
          MEMtoWB.fwb = 0;
          break;
        case 0xb8000000:
          t = dcache_store_EXtoMEM.dval;
          n = dcache_store_EXtoMEM.n;
          offset = SIGNEXTEND(dcache_store_EXtoMEM.imm1);
          cache_write(n + offset, t);
          MEMtoWB.fwb = 0;
          break;
        case 0x38000000:
          t = dcache_store_EXtoMEM.dval;
          n = dcache_store_EXtoMEM.n;
          offset = SIGNEXTEND(dcache_store_EXtoMEM.imm1);
          load = cache_read(n + offset, 8);
          load = (load & 0xffffff00) | (int)t;
          cache_write(n + offset, load);
          MEMtoWB.fwb = 0;
          break;
        case 0x78000000:
          t = dcache_store_EXtoMEM.dval;
          n = dcache_store_EXtoMEM.n;
          offset = SIGNEXTEND(dcache_store_EXtoMEM.imm1);
          load = cache_read(n + offset, 8);
          //original implementation
          // load = (load & 0xffffff00) | (t & 0x0000ffff);
          load = t & 0x0000ffff;
          cache_write(n + offset, load);
          MEMtoWB.fwb = 0;
          break;
      }
      MEMtoWB.dnum = EXtoMEM.dnum;
      MEMtoWB.fn = EXtoMEM.fn;
      MEMtoWB.fz = EXtoMEM.fz;
      MEMtoWB.fmem = EXtoMEM.fmem;
      return;
    }

  }
  if (EXtoMEM.halt == 1)
  {
    MEMtoWB.halt = 1;
    MEMtoWB.fn = EXtoMEM.fn;
    MEMtoWB.fz = EXtoMEM.fz;
    return;
  }
  if (EXtoMEM.fmem) {
    switch (EXtoMEM.op)
    {
      case 0xf8400000:
        LDUR();
        break;
      case 0xb8400000:
        LDUR2();
        break;
      case 0x38400000:
        LDURB();
        break;
      case 0x78400000:
        LDURH();
        break;
      case 0xf8000000:
        STUR();
        break;
      case 0xb8000000:
        STUR2();
        break;
      case 0x38000000:
        STURB();
        break;
      case 0x78000000:
        STURH();
        break;
    }
    MEMtoWB.dnum = EXtoMEM.dnum;
    MEMtoWB.fn = EXtoMEM.fn;
    MEMtoWB.fz = EXtoMEM.fz;
    MEMtoWB.fmem = EXtoMEM.fmem;
    //same cycle as bubble_trigger save EXtoMEM in a temp struct
    if (Control.data_cache_bubble == 51)
    {
      dcache_store_EXtoMEM = (EXtoMEM_t) {.op = EXtoMEM.op, .n = EXtoMEM.n, .dnum = EXtoMEM.dnum, .dval = EXtoMEM.dval, .imm1 = EXtoMEM.imm1, .res = EXtoMEM.res, .fmem = EXtoMEM.fmem, .fn = EXtoMEM.fn, .fz = EXtoMEM.fz};
    }
  } else {
    MEMtoWB = (MEMtoWB_t){ .op = EXtoMEM.op, .dnum = EXtoMEM.dnum, .res = EXtoMEM.res, .fwb = EXtoMEM.fwb, .fn = EXtoMEM.fn, .fz = EXtoMEM.fz, .branching = EXtoMEM.branching};
  }
  EXtoMEM = (EXtoMEM_t){ .n = 0, .dnum = 0, .dval = 0, .imm1 = 0, .res = 0, .fmem = 0, .fn = 0, .fz = 0};
}

void pipe_stage_execute()
{
  //first and middle cycles of data_cache bubble do nothing
  if (Control.data_cache_bubble == 51)
  {
  }
  else if (Control.data_cache_bubble > 0)
  {
    return;

  }
  // we have modify implementations to write to EXtoMEM.res
  if (!(IDtoEX.fmem)) {
    switch(IDtoEX.op)
    {
        // Add/Subtract immediate
        case 0x91000000:
          ADD_Immediate();
          break;
        case 0xb1000000:
          ADDS_Immediate();
          break;
        case 0xd1000000:
          SUB_Immediate();
          break;
        case 0xf1000000:
          SUBS_Immediate();
          break;
        // Compare and branch
        case 0xb4000000:
          CBZ();
          break;
        case 0xb5000000:
          CBNZ();
          break;
        // Move wide
        case 0xd2800000:
          MOVZ();
          break;
        // Bitfield
        case 0xd3000000:
          BITSHIFT();
          break;
        // Conditional branch
        case 0x54000000:
          B_Cond();
          break;
        // Exceptions
        case 0xd4400000:
          HLT();
          break;
        // Unconditional branch (register)
        case 0xd61f0000:
          BR();
          break;
        // Unconditional branch (immediate)
        case 0x14000000:
          B();
          break;

        // Logical (shifted register)
        case 0x8a000000:
          AND();
          break;
        case 0xea000000:
          ANDS();
          break;
        case 0xca000000:
          EOR();
          break;
        case 0xaa000000:
          ORR();
          break;
        // Add/subtract (extended)
        case 0x8b000000:
          ADD_Extended();
          break;
        case 0xab000000:
          ADDS_Extended();
          break;
        case 0xcb000000:
          SUB_Extended();
          break;
        case 0xeb000000:
          SUBS_Extended();
          break;
        // Data Processing (3 source)
        case 0x9b000000:
          MUL();
          break;
    }
  } else {
    EXtoMEM.op = IDtoEX.op;
    EXtoMEM.n = IDtoEX.n;
    EXtoMEM.dval = IDtoEX.dval;
    EXtoMEM.imm1 = IDtoEX.imm1;
  }
  EXtoMEM.op = IDtoEX.op;
  EXtoMEM.dnum = IDtoEX.dnum;
  EXtoMEM.fwb = IDtoEX.fwb;
  EXtoMEM.fmem = IDtoEX.fmem;
  EXtoMEM.branching = IDtoEX.branching;
  EXtoMEM.fn = Control.fn;
  EXtoMEM.fz = Control.fz;

  //dont clear IDtoEX if we are in loadstore_bubble, do clear if we are in branch_bubble
  if ((int) stat_cycles <= Control.loadstore_bubble_until)
  {
    return;
  }
  else
  {
    IDtoEX = (IDtoEX_t){ .op = 0, .m = 0, .n = 0, .dnum = 0, .imm1 = 0, .imm2 = 0, .addr = 0, .fmem = 0, .fwb = 0};
  }
}

void pipe_stage_decode()
{
  //first and middle cycles of data_cache bubble do nothing
  if (Control.data_cache_bubble == 51)
  {
  }
  else if (Control.data_cache_bubble > 0)
  {
    return;

  }

  //loadstore bubbling detection
  if (EXtoMEM.op == 0xf8400000)
  {
    loadstore_dependency = 1;
  }
  //in loadstore bubble rn
  if ((int)stat_cycles < Control.loadstore_bubble_until)
  {
    return;
  }
  //loadstore bubbling restore to pipeline
  if ((int) stat_cycles == Control.loadstore_bubble_until)
  {
    Control.loadstore_bubble_until = -1;
    Control.restoration = 1;
    IDtoEX = (IDtoEX_t){.op = temp_IDtoEX.op, .m = temp_IDtoEX.m, .n = temp_IDtoEX.n, .dnum = temp_IDtoEX.dnum, .imm1 = temp_IDtoEX.imm1, .imm2 = temp_IDtoEX.imm2, .addr = temp_IDtoEX.addr, .fmem = temp_IDtoEX.fmem, .fwb = temp_IDtoEX.fwb};
    IDtoEX.n = reg_call(IDtoEX.n);
    IDtoEX.m = reg_call(IDtoEX.m);
    IFtoID = (IFtoID_t){.inst = temp_IFtoID.inst};
    return;
  }

  uint32_t word = IFtoID.inst;
  int temp = word & 0x1E000000;
  // Data Processing - Immediate
  if ((temp == 0x10000000) || (temp == 0x12000000)) {
    int temp2 = word & 0x03800000;
    // Add/subtract (immediate)
    if ((temp2 == 0x01000000) || (temp2 == 0x01800000)) {
      IDtoEX.op = (word & 0xff000000);
      IDtoEX.imm1 = (word & 0x003ffc00) >> 10;
      IDtoEX.dnum = (word & 0x0000001f);
      IDtoEX.fwb = 1;
      // loadstore bubbling trigger
      if (loadstore_dependency == 1)
      {
        IDtoEX.n = ((word & 0x000003e0) >> 5);
        if (IDtoEX.n == EXtoMEM.dnum)
        {
          TriggerBubble_LoadStore((int) stat_cycles + 1);
          loadstore_dependency = 0;
        }
        else
        {
          loadstore_dependency = 0;
          IDtoEX.n = reg_call(((word & 0x000003e0) >> 5));
        }

      }
      else
      {
        IDtoEX.n = reg_call(((word & 0x000003e0) >> 5));
      }
    }
    // Move wide (immediate)
    else if (temp2 == 0x02800000) {
      IDtoEX.op = (word & 0xff800000);
      IDtoEX.imm1 = (word & 0x001fffe0) >> 5;
      IDtoEX.dnum = (word & 0x0000001f);
      IDtoEX.fwb = 1;
    }
    // Bitfield
    else if (temp2 == 0x03000000) {
      // this should be reviewed, since i am unsure what you guys need - victor
      IDtoEX.op = (word & 0xff800000);
      // IDtoEX.n = reg_call((word & 0x000003e0) >> 5);
      IDtoEX.dnum = (word & 0x0000001f);
      IDtoEX.imm1 = (word & 0x0000fc00) >> 10;
      IDtoEX.imm2 = (word & 0x003f0000) >> 16; // using m as another imm
      IDtoEX.fwb = 1;
       // loadstore bubbling trigger
      if (loadstore_dependency == 1)
      {
        IDtoEX.n = ((word & 0x000003e0) >> 5);
        if (IDtoEX.n == EXtoMEM.dnum)
        {
          TriggerBubble_LoadStore((int) stat_cycles + 1);
          loadstore_dependency = 0;
        }
        else
        {
          loadstore_dependency = 0;
          IDtoEX.n = reg_call(((word & 0x000003e0) >> 5));
        }

      }
      else
      {
        IDtoEX.n = reg_call(((word & 0x000003e0) >> 5));
      }
    }
    else {
    }
  }

  // Branches and Exceptions
  else if ((temp == 0x14000000) || (temp == 0x16000000)) {
    // Conditional branch
    if ((word & 0xfe000000) == 0x54000000) {
      IDtoEX.op = (word & 0xff000000);
      //check d for cond
      IDtoEX.dnum = (word & 0x0000000f);
      //IDtoEX.imm = (word & 0x00ffffe0) >> 5;
      IDtoEX.addr = ((word & 0x00FFFFE0) | ((word & 0x800000) ? 0xFFFFFFFFFFF80000 : 0));
      IDtoEX.branching = 1;
      // TriggerBubble_Branch((int) stat_cycles + 2);
      // if (Control.prediction_taken != 1)
      // {condBubble(IDtoEX.dnum);}
    }
    // Exception
    else if ((word & 0xff000000) == 0xd4000000) {
      IDtoEX.op = (word & 0xffe00000);
      IDtoEX.imm1 = (word & 0x001fffe0) >> 5;
      IDtoEX.branching = 1;
      // IFtoID.pc_halt = 1;
    }
    // Unconditional branch (register)
    else if ((word & 0xfe000000) == 0xd6000000) {
      IDtoEX.op = (word & 0xfffffc00);
      //IDtoEX.n = reg_call((word & 0x000003e0) >> 5);
      IDtoEX.n = (word & 0x000003e0) >> 5;
      IDtoEX.branching = 1;
      // if (Control.prediction_taken != 1)
      // {TriggerBubble_Branch((int) stat_cycles + 2);}
    }
    // Unconditional branch (immediate)
    else if ((word & 0x60000000) == 0) {
      IDtoEX.op = (word & 0xfc000000);
      //sign extending to 64 bits
      IDtoEX.addr = (word & 0x03ffffff) | ((word & 0x2000000) ? 0xFFFFFFFFFC000000 : 0);
      IDtoEX.branching = 1;
      // if (Control.prediction_taken != 1)
      // {TriggerBubble_Branch((int) stat_cycles + 2);}
    }
    // Compare and branch
    else if ((word & 0x7e000000) == 0x34000000) {
      IDtoEX.op = (word & 0xff000000);
      IDtoEX.dnum = (word & 0x0000001f);
      // IDtoEX.dval = reg_call(IDtoEX.dnum);
      IDtoEX.dval = CURRENT_STATE.REGS[(IDtoEX.dnum)];
      IDtoEX.addr = (word & 0x00ffffe0) | ((word & 0x800000) ? 0xFFFFFFFFFF000000 : 0);
      IDtoEX.branching = 1;
      // if (Control.prediction_taken != 1)
      // compareBubble(IDtoEX.dnum);
    }
    else {
    }
  }

  // Loads and Stores
  else if ((word & 0x0a000000) == 0x08000000) {
    IDtoEX.op = (word & 0xffc00000);
    // IDtoEX.n = reg_call((word & 0x000003e0) >> 5);
    IDtoEX.dnum = (word & 0x0000001f);
    IDtoEX.imm1 = (word & 0x001ff000) >> 12;
    IDtoEX.dval = reg_call(IDtoEX.dnum);
    IDtoEX.fmem = 1;
    // loadstore bubbling trigger
    if (loadstore_dependency == 1)
    {
      IDtoEX.n = ((word & 0x000003e0) >> 5);
      if (IDtoEX.dnum == EXtoMEM.dnum)
      {
        TriggerBubble_LoadStore((int) stat_cycles + 1);
        loadstore_dependency = 0;
      }
      else
      {
        loadstore_dependency = 0;
        IDtoEX.n = reg_call(((word & 0x000003e0) >> 5));
      }

    }
    else
    {
      IDtoEX.n = reg_call(((word & 0x000003e0) >> 5));
    }
  }
  // Data Processing - Register
  else if ((word & 0x0e000000) == 0x0a000000) {
    // Logical (shifted register)
    if ((word & 0x1f000000) == 0x0a000000) {
      IDtoEX.op = (word & 0xff000000);
      // IDtoEX.m = reg_call((word & 0x001f0000) >> 16);
      // IDtoEX.n = reg_call((word & 0x000003e0) >> 5);
      IDtoEX.dnum = (word & 0x0000001f);
      IDtoEX.imm1 = (word & 0x0000fc00) >> 10;
      IDtoEX.fwb = 1;
      //loadstore bubble detection
      if (loadstore_dependency == 1)
      {
        IDtoEX.n = ((word & 0x000003e0) >> 5);
        IDtoEX.m = ((word & 0x001f0000) >> 16);
        if (IDtoEX.n == EXtoMEM.dnum || IDtoEX.m == EXtoMEM.dnum)
        {
          TriggerBubble_LoadStore((int) stat_cycles + 1);
          loadstore_dependency = 0;
        }
        else
        {
          loadstore_dependency = 0;
          IDtoEX.n = reg_call(((word & 0x000003e0) >> 5));
          IDtoEX.m = reg_call((word & 0x001f0000) >> 16);
        }

      }
      else
      {
        IDtoEX.n = reg_call(((word & 0x000003e0) >> 5));
        IDtoEX.m = reg_call((word & 0x001f0000) >> 16);
      }
    }
    // Add/subtract (extended register)
    else if ((word & 0x1f200000) == 0x0b000000) {
      IDtoEX.op = (word & 0xffe00000);
      // IDtoEX.m = reg_call((word & 0x001f0000) >> 16);
      // IDtoEX.n = reg_call((word & 0x000003e0) >> 5);
      IDtoEX.dnum = (word & 0x0000001f);
      IDtoEX.fwb = 1;
      //loadstore bubble detection
      if (loadstore_dependency == 1)
      {
        IDtoEX.n = ((word & 0x000003e0) >> 5);
        IDtoEX.m = ((word & 0x001f0000) >> 16);
        if (IDtoEX.n == EXtoMEM.dnum || IDtoEX.m == EXtoMEM.dnum)
        {
          TriggerBubble_LoadStore((int) stat_cycles + 1);
          loadstore_dependency = 0;
        }
        else
        {
          loadstore_dependency = 0;
          IDtoEX.n = reg_call(((word & 0x000003e0) >> 5));
          IDtoEX.m = reg_call((word & 0x001f0000) >> 16);
        }

      }
      else
      {
        IDtoEX.n = reg_call(((word & 0x000003e0) >> 5));
        IDtoEX.m = reg_call((word & 0x001f0000) >> 16);
      }
    }
    // Data processing (3 source)
    else if ((word & 0x1f000000) == 0x1b000000) {
      IDtoEX.op = (word & 0xffe00000);
      // IDtoEX.m = reg_call((word & 0x001f0000) >> 16);
      // IDtoEX.n = reg_call((word & 0x000003e0) >> 5);
      IDtoEX.dnum = (word & 0x0000001f);
      IDtoEX.fwb = 1;
      //loadstore bubble detection
      if (loadstore_dependency == 1)
      {
        IDtoEX.n = ((word & 0x000003e0) >> 5);
        IDtoEX.m = ((word & 0x001f0000) >> 16);
        if (IDtoEX.n == EXtoMEM.dnum || IDtoEX.m == EXtoMEM.dnum)
        {
          TriggerBubble_LoadStore((int) stat_cycles + 1);
          loadstore_dependency = 0;
        }
        else
        {
          loadstore_dependency = 0;
          IDtoEX.n = reg_call(((word & 0x000003e0) >> 5));
          IDtoEX.m = reg_call((word & 0x001f0000) >> 16);
        }

      }
      else
      {
        IDtoEX.n = reg_call(((word & 0x000003e0) >> 5));
        IDtoEX.m = reg_call((word & 0x001f0000) >> 16);
      }
    }
    else {
    }
  }
  else {
  }
  // IFtoID = (IFtoID_t){ .inst = 0};
  loadstore_dependency = 0;

}

void pipe_stage_fetch()
{
  //data_cache bubbling
  //first and middle cycles of data_cache bubble do nothing
  if (Control.data_cache_bubble == 50)
  {
    if (Control.inst_cache_bubble > 0)
    {
      Control.inst_cache_bubble -= 1;
    }
    Control.data_cache_bubble -= 1;
  }
  else if (Control.data_cache_bubble > 0)
  {
    if (Control.inst_cache_bubble > 0)
    {
      Control.inst_cache_bubble -= 1;
    }
    Control.data_cache_bubble -= 1;
    return;
  }
  //exception control
  if (Control.halt == 1)
  {
    return;
  }
  //branch bubbling
  //lab3 bubble: one cycle halt on fetch
  if (Control.lab3_bubble == 1)
  {
    Control.lab3_bubble = 0;
    return;
  }
  //branching bubble
  if (Control.cond_branch == 1)
  {
      CURRENT_STATE.PC = Control.baddr;
      Control.baddr = -1;
      //IFtoID.inst = mem_read_32(CURRENT_STATE.PC);
      // CURRENT_STATE.PC = CURRENT_STATE.PC + 4;
      Control.cond_branch = 0;
      if (Control.offsetlesshead_diff == 1)
      {
        Control.inst_cache_bubble = 0;
        Control.offsetlesshead_diff = 0;
      }
      if (Control.inst_cache_bubble > 0)
      {
        Control.inst_cache_bubble -= 1;
      }


      //cancel bubble according to Control.branch_offset-less_head
      return;
  }

  //lab2 loadstore bubbling
  //loadstore bubbling stop pc
  if ((int) stat_cycles <= Control.loadstore_bubble_until)
  {
    bp_predict(CURRENT_STATE.PC);
    return;
  }
  if (Control.restoration == 1)
  {

    Control.restoration = 0;
    return;
  }
  //handling branch after HLT
  // if (IDtoEX.op == 0xd4400000)
  // {
  //   CURRENT_STATE.PC = CURRENT_STATE.PC + 4;
  //   return;
  // }

  //inst cache bubble
  if (Control.inst_cache_bubble > 0)
  {
    //not last cycle of bubble
    if (Control.inst_cache_bubble > 1)
    {
      Control.inst_cache_bubble -= 1;
      return;
    }
    //last cycle, restore to pipeline
    else
    {
      Control.inst_cache_bubble = 0;
      return;
    }
  }

  uint32_t word = cache_read(CURRENT_STATE.PC, 4);
  //same cycle do nothing for bubble
  if (Control.inst_cache_bubble == 50)
  {
    Control.inst_cache_bubble -= 1;
    struct Prediction temp;
		temp.prediction_taken = 0;
		temp.pc_before_prediction = CURRENT_STATE.PC;
    enqueue(&q, temp);
    return;
  }
  //not in bubble(first cycle or mid-bubble or last-cycle) load word into struct to continue pipeline
  else
  {
    IFtoID.inst = word;
    //cache_read returns 0 if there is a branch in ID, so we don't do anything, sending a zero to IFtoID.inst is equivalent to a flush.
    // if (word == 0)
    // {
    //   return;
    // }
    //normal behavior
    bp_predict(CURRENT_STATE.PC);
  }
}

void TriggerBubble_Branch(int bubble_until)
{
  Control.branch_bubble_until = bubble_until;
  // CURRENT_STATE.PC = CURRENT_STATE.PC + 4;
  return;
}
void TriggerBubble_LoadStore(int bubble_until)
{
  Control.loadstore_bubble_until = bubble_until ;
  // Control.loadstore_bubble_start = bubble_until - 1;
  temp_IDtoEX = (IDtoEX_t){.op = IDtoEX.op, .m = IDtoEX.m, .n = IDtoEX.n, .dnum = IDtoEX.dnum, .imm1 = IDtoEX.imm1, .imm2 = IDtoEX.imm2, .addr = IDtoEX.addr, .fmem = IDtoEX.fmem, .fwb = IDtoEX.fwb};
  IDtoEX = (IDtoEX_t){ .op = 0, .m = 0, .n = 0, .dnum = 0, .imm1 = 0, .imm2 = 0, .addr = 0, .fmem = 0, .fwb = 0};
  temp_IFtoID = (IFtoID_t){.inst = mem_read_32(CURRENT_STATE.PC)};
  IFtoID = (IFtoID_t){ .inst = 0};
  return;
}
/* Queue functions*/
struct entry* newentry(struct Prediction pred)
{
    struct entry* temp = (struct entry*)malloc(sizeof(struct entry));
    temp->pred = pred;
    return temp;
}

struct entry* dequeue(struct queue* q)
{
    //empty q
    if (q->head == NULL)
        return NULL;

    //just head
    else if ((q->head != NULL) && (q->tail == NULL))
    {
      struct entry* temp = q->head;
      q->head = NULL;
      return temp;
    }
    //head and tail
    else
    {
      struct entry* temp = q->head;
      q->head = q->tail;
      q->tail = NULL;
      return temp;
    }
}

void decaptiate(struct queue* q)
{
    struct entry* temp = dequeue(q);
    free(temp);
}

struct queue* createqueue(int maxlen)
{
    struct queue* q = (struct queue*)malloc(sizeof(struct queue));
    q->maxlen = maxlen;
    q->currlen = 0;
    q->head = NULL;
    q->tail = NULL;
    return q;
}

void enqueue(struct queue* q, struct Prediction pred)
{
    struct entry* temp = newentry(pred);
    //empty
    if (q->head == NULL)
    {
      q->head = temp;
      return;
    }
    //just head
    else if ((q->head != NULL) && (q->tail == NULL))
    {
      q->tail = temp;
      return;
    }
    //full, head and tail
    else
    {
      q->head = q->tail;
      q->tail = temp;
      return;
    }

}

void freequeue(struct queue* q)
{
    for (int i = 0; i < q->currlen; i++) {
        decaptiate(q);
    }

    free(q);
}

/* instruction implementations */
void Branch(int64_t offset, int64_t base)
{

    // uint64_t temp = CURRENT_STATE.PC - 8;
    Control.baddr = base + (offset * 4);


    // grab then squash, will need to restore to pipeline if PC + 4
    Control.squashed = IFtoID.inst;
    IFtoID = (IFtoID_t){ .inst = 0};

    return;
}
/*restore and flush function for lab3 */
void Restore_Flush(uint32_t real_target, int pred_taken, int branch_taken, uint32_t pc_before_prediction, uint64_t taken_target)
{
  if (pred_taken == 0)
  {
    if ((real_target == pc_before_prediction + 4) && (branch_taken == 1))
    {
      //prediction was correct, do nothing to pipeline but remember to reset lab3 control struct fields
      return;
    }
    else
    {
      //misprediction on PC + 4, don't do shit
      // CURRENT_STATE.PC = real_target;
      // IFtoID = (IFtoID_t){ .inst = 0};
      // Control.lab3_bubble = 1;
      //reset fields
      return;
    }
  }
  else if (pred_taken == 1)
  {
    // branch prediction success, don't do anything
    if ((real_target == taken_target) && (branch_taken == 1))
    {
      return;
    }
    else
    {
      // this aint working
      //misprediction, reset pc to pc before prediction, flush, and bubble
      CURRENT_STATE.PC = pc_before_prediction + 4;
      IFtoID = (IFtoID_t){ .inst = 0};
      Control.lab3_bubble = 1;
      return;
    }
  }
}

void CBNZ()
{
    int64_t offset = IDtoEX.addr/32;
    struct entry* temp_entry = dequeue(&q);
    int branch_taken;
    int reg_load_ahead = 0;
    //seeing if function ahead is a reg load
    switch(MEMtoWB.op)
    {
      // Add/Subtract immediate
        case 0x91000000:
          reg_load_ahead = 1;
          break;
        case 0xb1000000:
          reg_load_ahead = 1;
          break;
        case 0xd1000000:
          reg_load_ahead = 1;
          break;
        case 0xf1000000:
          reg_load_ahead = 1;
          break;
        // Compare and branch
        case 0xb4000000:
          break;
        case 0xb5000000:
          break;
        // Move wide
        case 0xd2800000:
          reg_load_ahead = 1;
          break;
        // Bitfield
        case 0xd3000000:
          break;
        // Conditional branch
        case 0x54000000:
          break;
        // Exceptions
        case 0xd4400000:
          break;
        // Unconditional branch (register)
        case 0xd61f0000:
          break;
        // Unconditional branch (immediate)
        case 0x14000000:
          break;

        // Logical (shifted register)
        case 0x8a000000:
          reg_load_ahead = 1;
          break;
        case 0xea000000:
          reg_load_ahead = 1;
          break;
        case 0xca000000:
          reg_load_ahead = 1;
          break;
        case 0xaa000000:
          reg_load_ahead = 1;
          break;
        // Add/subtract (extended)
        case 0x8b000000:
          reg_load_ahead = 1;
          break;
        case 0xab000000:
          reg_load_ahead = 1;
          break;
        case 0xcb000000:
          reg_load_ahead = 1;
          break;
        case 0xeb000000:
          reg_load_ahead = 1;
          break;
        // Data Processing (3 source)
        case 0x9b000000:
          reg_load_ahead = 1;
          break;
    }

    if (((MEMtoWB.dnum == IDtoEX.dnum) && (MEMtoWB.res != 0) && (reg_load_ahead == 1)))
    {
      branch_taken = 1;
    }
    else if (((MEMtoWB.dnum == IDtoEX.dnum) && (MEMtoWB.res == 0) && (reg_load_ahead == 1)))
    {
      branch_taken = 0;
    }
    else if (CURRENT_STATE.REGS[IDtoEX.dnum] != 0)
    {
      branch_taken = 1;
    }
    else
    {
      branch_taken = 0;
    }


    //lab2 behavior
    if (temp_entry->pred.prediction_taken == 0)
    {
      if (branch_taken == 0)
      {
        //don't branch
        // // grab then squash, will need to restore to pipeline if PC + 4
        // Control.not_taken = 1;
        // Control.squashed = IFtoID.inst;
        // IFtoID = (IFtoID_t){ .inst = 0};
      }
      else
      {
        //branch
        Control.cond_branch = 1;
        Branch(offset, temp_entry->pred.pc_before_prediction);
      }
      //bp_update
      int inc;
      (branch_taken == 1) ? (inc = 1) : (inc = -1);
      uint64_t temp = CURRENT_STATE.PC - 8;
      //(pc where argument was fetched, cond_bit, target addr, inc)
      bp_update(temp_entry->pred.pc_before_prediction, 1, (temp_entry->pred.pc_before_prediction + (offset * 4)), inc);
      Restore_Flush(temp_entry->pred.pc_before_prediction + (offset * 4), 0, branch_taken, temp_entry->pred.pc_before_prediction, temp_entry->pred.taken_target);
      return;
    }

    //prediction_taken behavior
    else if (temp_entry->pred.prediction_taken == 1)
    {
      //1. update
      int inc;
      (branch_taken == 1) ? (inc = 1) : (inc = -1);
      bp_update(temp_entry->pred.pc_before_prediction, 1, temp_entry->pred.pc_before_prediction + (offset * 4), inc);
      //2. restore and flush
      Restore_Flush(temp_entry->pred.pc_before_prediction + (offset * 4), 1, branch_taken, temp_entry->pred.pc_before_prediction, temp_entry->pred.taken_target);
      return;
    }

}
void CBZ()
{
    int64_t offset = IDtoEX.addr/32;
    struct entry* temp_entry = dequeue(&q);
    int branch_taken;
    int reg_load_ahead = 0;
    switch(MEMtoWB.op)
    {
      // Add/Subtract immediate
        case 0x91000000:
          reg_load_ahead = 1;
          break;
        case 0xb1000000:
          reg_load_ahead = 1;
          break;
        case 0xd1000000:
          reg_load_ahead = 1;
          break;
        case 0xf1000000:
          reg_load_ahead = 1;
          break;
        // Compare and branch
        case 0xb4000000:
          break;
        case 0xb5000000:
          break;
        // Move wide
        case 0xd2800000:
          reg_load_ahead = 1;
          break;
        // Bitfield
        case 0xd3000000:
          break;
        // Conditional branch
        case 0x54000000:
          break;
        // Exceptions
        case 0xd4400000:
          break;
        // Unconditional branch (register)
        case 0xd61f0000:
          break;
        // Unconditional branch (immediate)
        case 0x14000000:
          break;

        // Logical (shifted register)
        case 0x8a000000:
          reg_load_ahead = 1;
          break;
        case 0xea000000:
          reg_load_ahead = 1;
          break;
        case 0xca000000:
          reg_load_ahead = 1;
          break;
        case 0xaa000000:
          reg_load_ahead = 1;
          break;
        // Add/subtract (extended)
        case 0x8b000000:
          reg_load_ahead = 1;
          break;
        case 0xab000000:
          reg_load_ahead = 1;
          break;
        case 0xcb000000:
          reg_load_ahead = 1;
          break;
        case 0xeb000000:
          reg_load_ahead = 1;
          break;
        // Data Processing (3 source)
        case 0x9b000000:
          reg_load_ahead = 1;
          break;
    }

    if (((MEMtoWB.dnum == IDtoEX.dnum) && (MEMtoWB.res == 0) && (reg_load_ahead == 1)))
    {
      branch_taken = 1;
    }
    else if (((MEMtoWB.dnum == IDtoEX.dnum) && (MEMtoWB.res != 0) && (reg_load_ahead == 1)))
    {
      branch_taken = 0;
    }
    else if (CURRENT_STATE.REGS[IDtoEX.dnum] == 0)
    {
      branch_taken = 1;
    }
    else
    {
      branch_taken = 0;
    }

    //lab2 behavior
    if (temp_entry->pred.prediction_taken == 0)
    {
      if (branch_taken == 0)
      {
        //don't branch
        // // grab then squash, will need to restore to pipeline if PC + 4
        // Control.not_taken = 1;
        // Control.squashed = IFtoID.inst;
        // IFtoID = (IFtoID_t){ .inst = 0};
      }
      else
      {
        //branch
        Control.cond_branch = 1;
        Branch(offset, temp_entry->pred.pc_before_prediction);
      }
      //bp_update
      int inc;
      (branch_taken == 1) ? (inc = 1) : (inc = -1);
      uint64_t temp = CURRENT_STATE.PC - 8;
      //(pc where argument was fetched, cond_bit, target addr, inc)
      bp_update(temp_entry->pred.pc_before_prediction, 1, (temp_entry->pred.pc_before_prediction + (offset * 4)), inc);
      Restore_Flush(temp_entry->pred.pc_before_prediction + (offset * 4), 0, branch_taken, temp_entry->pred.pc_before_prediction, temp_entry->pred.taken_target);
      return;
    }

    //prediction_taken behavior
    else if (temp_entry->pred.prediction_taken == 1)
    {
      //1. update
      int inc;
      (branch_taken == 1) ? (inc = 1) : (inc = -1);
      bp_update(temp_entry->pred.pc_before_prediction, 1, temp_entry->pred.pc_before_prediction + (offset * 4), inc);
      //2. restore and flush
      Restore_Flush(temp_entry->pred.pc_before_prediction + (offset * 4), 1, branch_taken, temp_entry->pred.pc_before_prediction, temp_entry->pred.taken_target);
      return;
    }
}
void MUL()
{
  if (IDtoEX.dnum != 31)
  {
    EXtoMEM.dnum = IDtoEX.dnum;
    EXtoMEM.res = IDtoEX.n * IDtoEX.m;
  }
  //set fields to tell MEM it shouldn't do anything
  return;
}
void HLT()
{
    EXtoMEM.halt = 1;
    Control.halt = 1;
    // CURRENT_STATE.PC = CURRENT_STATE.PC + 4;
    return;
}
void BR()
{
    //IDtoEX.n is Reg Number, not Reg content
    int64_t direct_target;
    struct entry* temp_entry = dequeue(&q);
    // instruction ahead of us is storing in the same Reg that we want to read from but it hasn't written back yet, so we use it's value when its at the MEM stage
    if (MEMtoWB.dnum == IDtoEX.n)
    {
      direct_target = MEMtoWB.res;
    }
    else
    {
      direct_target = CURRENT_STATE.REGS[IDtoEX.n];
    }

    if (temp_entry->pred.prediction_taken == 0)
    {
      Control.cond_branch = 1;

      // this is the branch part, can't use Branch() tho cause that one assumes input is an offset
      Control.baddr = direct_target;
      // grab then squash, will need to restore to pipeline if PC + 4
      Control.squashed = IFtoID.inst;
      IFtoID = (IFtoID_t){ .inst = 0};

      //bp_update part
      uint64_t temp = CURRENT_STATE.PC - 8;
      //(pc where argument was fetched, cond_bit, target addr, inc)
      bp_update(temp_entry->pred.pc_before_prediction, 0, direct_target, 1);
      Restore_Flush(direct_target, 0, 1, temp_entry->pred.pc_before_prediction, temp_entry->pred.taken_target);
    }
    else if (temp_entry->pred.prediction_taken == 1)
    {
      bp_update(temp_entry->pred.pc_before_prediction, 0, direct_target, 1);
      Restore_Flush(direct_target, 1, 1, temp_entry->pred.pc_before_prediction, temp_entry->pred.taken_target);
    }
    return;
}
void B()
{

    int64_t offset = IDtoEX.addr;
    struct entry* temp_entry = dequeue(&q);
    if (temp_entry->pred.prediction_taken == 0)
    {
      Control.cond_branch = 1;
      Branch(offset, temp_entry->pred.pc_before_prediction);
      uint64_t temp = CURRENT_STATE.PC - 8;
      //(pc where argument was fetched, cond_bit, target addr, inc)
      bp_update(temp_entry->pred.pc_before_prediction, 0, (temp_entry->pred.pc_before_prediction + (offset * 4)), 1);
      // real target, pred_taken, branch_taken
      Restore_Flush(temp_entry->pred.pc_before_prediction + (offset * 4), 0, 1, temp_entry->pred.pc_before_prediction, temp_entry->pred.taken_target);
      return;
    }
    else if (temp_entry->pred.prediction_taken == 1)
    {
      //bp update and flush
      bp_update(temp_entry->pred.pc_before_prediction, 0, temp_entry->pred.pc_before_prediction + (offset * 4), 1);
      //2. restore and flush
      Restore_Flush(temp_entry->pred.pc_before_prediction + (offset * 4), 1, 1, temp_entry->pred.pc_before_prediction, temp_entry->pred.taken_target);
    }
}
void B_Cond()
{
    //b_cond
    int64_t cond = IDtoEX.dnum;
    int64_t offset = IDtoEX.addr >> 5;
    struct entry* temp_entry = dequeue(&q);
    int branch_taken;
    switch(cond)
    {
        //BEQ
        case(0):
            if (Control.fz == 1)
            {
              branch_taken = 1;
            }
            else
            {
              branch_taken = 0;
            }

            if (temp_entry->pred.prediction_taken == 0)
            {
              //lab2 behavior
              if (branch_taken == 1)
              {
                Branch(offset, temp_entry->pred.pc_before_prediction);
                Control.cond_branch = 1;
              }
              else if (Control.branch_bubble_until != -1)
              {
                Control.not_taken = 1;
                // grab then squash, will need to restore to pipeline if PC + 4
                Control.squashed = IFtoID.inst;
                IFtoID = (IFtoID_t){ .inst = 0};
              }
              //bp update and flush
              int inc;
              (branch_taken == 1) ? (inc = 1) : (inc = -1);
              uint64_t temp = CURRENT_STATE.PC - 8;
              //(pc where argument was fetched, cond_bit, target addr, inc)
              bp_update(temp_entry->pred.pc_before_prediction, 1, (temp_entry->pred.pc_before_prediction + (offset * 4)), inc);
              Restore_Flush(temp_entry->pred.pc_before_prediction + (offset * 4), 0, branch_taken, temp_entry->pred.pc_before_prediction, temp_entry->pred.taken_target);
              break;
            }
            else if (temp_entry->pred.prediction_taken == 1)
            {
              //bp update and flush
              int inc;
              (branch_taken == 1) ? (inc = 1) : (inc = -1);
              bp_update(temp_entry->pred.pc_before_prediction, 1, temp_entry->pred.pc_before_prediction + (offset * 4), inc);
              //2. restore and flush
              Restore_Flush(temp_entry->pred.pc_before_prediction + (offset * 4), 1, branch_taken, temp_entry->pred.pc_before_prediction, temp_entry->pred.taken_target);
              break;
            }
        //BNE
        case(1):
            if (Control.fz == 0)
            {
              branch_taken = 1;
            }
            else
            {
              branch_taken = 0;
            }

            if (temp_entry->pred.prediction_taken == 0)
            {
              //lab2 behavior
              if (branch_taken == 1)
              {
                Branch(offset, temp_entry->pred.pc_before_prediction);
                Control.cond_branch = 1;
              }
              else if (Control.branch_bubble_until != -1)
              {
                Control.not_taken = 1;
                // grab then squash, will need to restore to pipeline if PC + 4
                Control.squashed = IFtoID.inst;
                IFtoID = (IFtoID_t){ .inst = 0};
              }
              //bp update and flush
              int inc;
              (branch_taken == 1) ? (inc = 1) : (inc = -1);
              uint64_t temp = CURRENT_STATE.PC - 8;
              //(pc where argument was fetched, cond_bit, target addr, inc)
              bp_update(temp_entry->pred.pc_before_prediction, 1, (temp_entry->pred.pc_before_prediction + (offset * 4)), inc);
              Restore_Flush(temp_entry->pred.pc_before_prediction + (offset * 4), 0, branch_taken, temp_entry->pred.pc_before_prediction, temp_entry->pred.taken_target);
              break;
            }
            else if (temp_entry->pred.prediction_taken == 1)
            {
              //bp update and flush
              int inc;
              (branch_taken == 1) ? (inc = 1) : (inc = -1);
              bp_update(temp_entry->pred.pc_before_prediction, 1, temp_entry->pred.pc_before_prediction + (offset * 4), inc);
              //2. restore and flush
              Restore_Flush(temp_entry->pred.pc_before_prediction + (offset * 4), 1, branch_taken, temp_entry->pred.pc_before_prediction, temp_entry->pred.taken_target);
              break;
            }


        //BGE
        case(10):
            if ((Control.fz == 1) || (Control.fn == 0))
            {
              branch_taken = 1;
            }
            else
            {
              branch_taken = 0;
            }
            if (temp_entry->pred.prediction_taken == 0)
            {
              //lab2 behavior
              if (branch_taken == 1)
              {
                Branch(offset, temp_entry->pred.pc_before_prediction);
                Control.cond_branch = 1;
              }
              else if (Control.branch_bubble_until != -1)
              {
                Control.not_taken = 1;
                // grab then squash, will need to restore to pipeline if PC + 4
                Control.squashed = IFtoID.inst;
                IFtoID = (IFtoID_t){ .inst = 0};
              }
              //bp update and flush
              int inc;
              (branch_taken == 1) ? (inc = 1) : (inc = -1);
              uint64_t temp = CURRENT_STATE.PC - 8;
              //(pc where argument was fetched, cond_bit, target addr, inc)
              bp_update(temp_entry->pred.pc_before_prediction, 1, (temp_entry->pred.pc_before_prediction + (offset * 4)), inc);
              Restore_Flush(temp_entry->pred.pc_before_prediction + (offset * 4), 0, branch_taken, temp_entry->pred.pc_before_prediction, temp_entry->pred.taken_target);
              break;
            }
            else if (temp_entry->pred.prediction_taken == 1)
            {
              //bp update and flush
              int inc;
              (branch_taken == 1) ? (inc = 1) : (inc = -1);
              bp_update(temp_entry->pred.pc_before_prediction, 1, temp_entry->pred.pc_before_prediction + (offset * 4), inc);
              //2. restore and flush
              Restore_Flush(temp_entry->pred.pc_before_prediction + (offset * 4), 1, branch_taken, temp_entry->pred.pc_before_prediction, temp_entry->pred.taken_target);
              break;
            }
        //BLT
        case(11):
            if ((Control.fn == 1) && (Control.fz == 0))
            {
              branch_taken = 1;
            }
            else
            {
              branch_taken = 0;
            }
            if (temp_entry->pred.prediction_taken == 0)
            {
              //lab2 behavior
              if (branch_taken == 1)
              {
                Branch(offset, temp_entry->pred.pc_before_prediction);
                Control.cond_branch = 1;
              }
              else if (Control.branch_bubble_until != -1)
              {
                Control.not_taken = 1;
                // grab then squash, will need to restore to pipeline if PC + 4
                Control.squashed = IFtoID.inst;
                IFtoID = (IFtoID_t){ .inst = 0};
              }
              //bp update and flush
              int inc;
              (branch_taken == 1) ? (inc = 1) : (inc = -1);
              uint64_t temp = CURRENT_STATE.PC - 8;
              //(pc where argument was fetched, cond_bit, target addr, inc)
              bp_update(temp_entry->pred.pc_before_prediction, 1, (temp_entry->pred.pc_before_prediction + (offset * 4)), inc);
              Restore_Flush(temp_entry->pred.pc_before_prediction + (offset * 4), 0, branch_taken, temp_entry->pred.pc_before_prediction, temp_entry->pred.taken_target);
              break;
            }
            else if (temp_entry->pred.prediction_taken == 1)
            {
              //bp update and flush
              int inc;
              (branch_taken == 1) ? (inc = 1) : (inc = -1);
              bp_update(temp_entry->pred.pc_before_prediction, 1, temp_entry->pred.pc_before_prediction + (offset * 4), inc);
              //2. restore and flush
              Restore_Flush(temp_entry->pred.pc_before_prediction + (offset * 4), 1, branch_taken, temp_entry->pred.pc_before_prediction, temp_entry->pred.taken_target);
              break;
            }
        //BGT
        case(12):
            if ((Control.fn == 0) && (Control.fz == 0))
            {
              branch_taken = 1;
            }
            else
            {
              branch_taken = 0;
            }
            if (temp_entry->pred.prediction_taken == 0)
            {
              //lab2 behavior
              if (branch_taken == 1)
              {
                Branch(offset, temp_entry->pred.pc_before_prediction);
                Control.cond_branch = 1;
              }
              else if (Control.branch_bubble_until != -1)
              {
                Control.not_taken = 1;
                // grab then squash, will need to restore to pipeline if PC + 4
                Control.squashed = IFtoID.inst;
                IFtoID = (IFtoID_t){ .inst = 0};
              }
              //bp update and flush
              int inc;
              (branch_taken == 1) ? (inc = 1) : (inc = -1);
              uint64_t temp = CURRENT_STATE.PC - 8;
              //(pc where argument was fetched, cond_bit, target addr, inc)
              bp_update(temp_entry->pred.pc_before_prediction, 1, (temp_entry->pred.pc_before_prediction + (offset * 4)), inc);
              Restore_Flush(temp_entry->pred.pc_before_prediction + (offset * 4), 0, branch_taken, temp_entry->pred.pc_before_prediction, temp_entry->pred.taken_target);
              break;
            }
            else if (temp_entry->pred.prediction_taken == 1)
            {
              //bp update and flush
              int inc;
              (branch_taken == 1) ? (inc = 1) : (inc = -1);
              bp_update(temp_entry->pred.pc_before_prediction, 1, temp_entry->pred.pc_before_prediction + (offset * 4), inc);
              //2. restore and flush
              Restore_Flush(temp_entry->pred.pc_before_prediction + (offset * 4), 1, branch_taken, temp_entry->pred.pc_before_prediction, temp_entry->pred.taken_target);
              break;
            }
        //BLE
        case(13):
            if ((Control.fz == 1) || (Control.fn == 1))
            {
              branch_taken = 1;
            }
            else
            {
              branch_taken = 0;
            }
            if (temp_entry->pred.prediction_taken == 0)
            {
              //lab2 behavior
              if (branch_taken == 1)
              {
                Branch(offset, temp_entry->pred.pc_before_prediction);
                Control.cond_branch = 1;
              }
              else if (Control.branch_bubble_until != -1)
              {
                Control.not_taken = 1;
                // grab then squash, will need to restore to pipeline if PC + 4
                Control.squashed = IFtoID.inst;
                IFtoID = (IFtoID_t){ .inst = 0};
              }
              //bp update and flush
              int inc;
              (branch_taken == 1) ? (inc = 1) : (inc = -1);
              uint64_t temp = CURRENT_STATE.PC - 8;
              //(pc where argument was fetched, cond_bit, target addr, inc)
              bp_update(temp_entry->pred.pc_before_prediction, 1, (temp_entry->pred.pc_before_prediction + (offset * 4)), inc);
              Restore_Flush(temp_entry->pred.pc_before_prediction + (offset * 4), 0, branch_taken, temp_entry->pred.pc_before_prediction, temp_entry->pred.taken_target);
              break;
            }
            else if (temp_entry->pred.prediction_taken == 1)
            {
              //bp update and flush
              int inc;
              (branch_taken == 1) ? (inc = 1) : (inc = -1);
              bp_update(temp_entry->pred.pc_before_prediction, 1, temp_entry->pred.pc_before_prediction + (offset * 4), inc);
              //2. restore and flush
              Restore_Flush(temp_entry->pred.pc_before_prediction + (offset * 4), 1, branch_taken, temp_entry->pred.pc_before_prediction, temp_entry->pred.taken_target);
              break;
            }
    }
    //Branch Helper Function already sets .branching to 1
    return;
}
void LDUR() {
  int64_t t = EXtoMEM.dnum;
  int64_t n = EXtoMEM.n;
  int64_t offset = SIGNEXTEND(EXtoMEM.imm1);
  int64_t load = cache_read(n + offset, 8);
  int64_t load2 = cache_read(n + offset + 4, 8);
  load = load | (load2 << 32);
  if (t != 31) {
    MEMtoWB.res = load;
  }
  MEMtoWB.fwb = 1;

}
void LDUR2() {
  int64_t t = EXtoMEM.dnum;
  int64_t n = EXtoMEM.n;
  int64_t offset = SIGNEXTEND(EXtoMEM.imm1);
  int64_t load = cache_read(n + offset, 8);
  if (t != 31) {
    MEMtoWB.res = load;
  }
  MEMtoWB.fwb = 1;

}
void LDURB() {
  int64_t t = EXtoMEM.dnum;
  int64_t n = EXtoMEM.n;
  int64_t offset = SIGNEXTEND(EXtoMEM.imm1);
  int load = cache_read(n + offset, 8);
  load = (load & 0x000000ff);
  if (t != 31) {
    MEMtoWB.res = load;
  }
  MEMtoWB.fwb = 1;

}
void LDURH() {
  int64_t t = EXtoMEM.dnum;
  int64_t n = EXtoMEM.n;
  int64_t offset = SIGNEXTEND(EXtoMEM.imm1);
  int load = cache_read(n + offset, 8);
  load = (load & 0x0000ffff);
  if (t != 31) {
    MEMtoWB.res = load;
  }
  MEMtoWB.fwb = 1;

}
void STUR() {
  int64_t t = EXtoMEM.dval;
  int64_t n = EXtoMEM.n;
  int64_t offset = SIGNEXTEND(EXtoMEM.imm1);
  cache_write(n + offset, (t & 0x00000000ffffffff));
  cache_write(n + offset + 4, ((t & 0xffffffff00000000) >> 32));
  MEMtoWB.fwb = 0;

}
void STUR2() {
  int64_t t = EXtoMEM.dval;
  int64_t n = EXtoMEM.n;
  int64_t offset = SIGNEXTEND(EXtoMEM.imm1);
  cache_write(n + offset, t);
  MEMtoWB.fwb = 0;

}
void STURB() {
  char t = EXtoMEM.dval;
  int64_t n = EXtoMEM.n;
  int64_t offset = SIGNEXTEND(EXtoMEM.imm1);
  int load = cache_read(n + offset, 8);
  load = (load & 0xffffff00) | (int)t;
  cache_write(n + offset, load);
  MEMtoWB.fwb = 0;

}
void STURH() {
  int t = EXtoMEM.dval;
  int64_t n = EXtoMEM.n;
  int64_t offset = SIGNEXTEND(EXtoMEM.imm1);
  int load = cache_read(n + offset, 8);
  //original implementation
  // load = (load & 0xffffff00) | (t & 0x0000ffff);
  load = t & 0x0000ffff;
  cache_write(n + offset, load);
  MEMtoWB.fwb = 0;

}
void ADD_Extended()
{
    int64_t n = IDtoEX.n;
    int64_t m = IDtoEX.m;
    EXtoMEM.res = n + m;

}
void ADD_Immediate()
{
    int64_t n = IDtoEX.n;
    int64_t imm = IDtoEX.imm1;
    EXtoMEM.res = n + imm;

}
void ADDS_Extended()
{
    int64_t n = IDtoEX.n;
    int64_t m = IDtoEX.m;
    int64_t res =  n + m;
    EXtoMEM.res = res;
    if (res == 0)
    {
        Control.fz = 1;
        Control.fn = 0;
    }
    else if (res < 0)
    {
        Control.fn = 1;
        Control.fz = 0;
    }
    else
    {
        Control.fz = 0;
        Control.fn = 0;
    }

}
void ADDS_Immediate()
{
    int64_t n = IDtoEX.n;
    int64_t imm = IDtoEX.imm1;
    int64_t res = n + imm;
    EXtoMEM.res = res;
    if (res == 0)
    {
        Control.fz = 1;
        Control.fn = 0;
    }
    else if (res < 0)
    {
        Control.fn = 1;
        Control.fz = 0;
    }
    else
    {
        Control.fz = 0;
        Control.fn = 0;
    }

}
void AND()
{
    int64_t n = IDtoEX.n;
    int64_t m = IDtoEX.m;
    EXtoMEM.res = n & m;

}
void ANDS()
{
    int64_t n = IDtoEX.n;
    int64_t m = IDtoEX.m;
    int64_t res = n & m;
    EXtoMEM.res = res;
    if (res == 0)
    {
        Control.fz = 1;
        Control.fn = 0;
    }
    else if (res < 0)
    {
        Control.fn = 1;
        Control.fz = 0;
    }
    else
    {
        Control.fz = 0;
        Control.fn = 0;
    }

}
void EOR()
{
    int64_t n = IDtoEX.n;
    int64_t m = IDtoEX.m;
    EXtoMEM.res = n ^ m;

}
void ORR()
{
    int64_t n = IDtoEX.n;
    int64_t m = IDtoEX.m;
    EXtoMEM.res = n | m;

}
void BITSHIFT()
{
  int64_t n = IDtoEX.n;
  int64_t immr = IDtoEX.imm2;
  int64_t res;
  if (IDtoEX.imm1 == 63) { // LSR
    res = n >> immr;
  }
  else { // LSL
    res = n << (64 - immr);
  }
  EXtoMEM.res = res;

}
void MOVZ()
{
  EXtoMEM.res = IDtoEX.imm1;
}
void SUB_Immediate()
{
    EXtoMEM.res = IDtoEX.n -IDtoEX.imm1;

}
void SUB_Extended()
{
    EXtoMEM.res = IDtoEX.n - IDtoEX.m;

}
void SUBS_Immediate()
{
    int64_t res = IDtoEX.n - IDtoEX.imm1;
    if (res == 0)
    {
        Control.fz = 1;
        Control.fn = 0;
    }
    else if (res < 0)
    {
        Control.fn = 1;
        Control.fz = 0;
    }
    else
    {
        Control.fn = 0;
        Control.fz = 0;
    }
    EXtoMEM.res = res;
}
void SUBS_Extended()
{
    int64_t res = IDtoEX.n - IDtoEX.m;
    if (res == 0)
    {
        Control.fz = 1;
        Control.fn = 0;
    }
    else if (res < 0)
    {
        Control.fn = 1;
        Control.fz = 0;
    }
    else
    {
        Control.fn = 0;
        Control.fz = 0;
    }
    EXtoMEM.res = res;
}
