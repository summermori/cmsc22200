/*
 * CMSC 22200
 *
 * ARM pipeline timing simulator
 */

#include "pipe.h"
#include "shell.h"
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
Control_t Control = {.baddr= -1, .bpc = 0, .branch_bubble_until = -1, .branch_grab = 0, .loadstore_bubble_until = -1, .restoration = -1, .fn = 0, .fz = 0, .halt = 0};
IDtoEX_t temp_IDtoEX;
IFtoID_t temp_IFtoID;
int loadstore_dependency = 0;
queue q = {.maxlen = 2, .currlen = 0, .head = NULL, .tail = NULL};


void compareBubble(int64_t reg_val)
{
  // printf("EXtoMEM.dnum: %ld\n", EXtoMEM.dnum);
  //only bubble on cbnz
  if ((EXtoMEM.dnum != reg_val) && (IDtoEX.op == 0xb5000000) && (CURRENT_STATE.REGS[reg_val] == 0))
  {
    // printf("CompareBubble Trigger\n");
    TriggerBubble_Branch((int) stat_cycles + 2);
  }
  return;
}

void condBubble(int64_t cond) // a helper to determine when conditional branching should bubble
{
  // printf("FN: %d\n", Control.fn);
  // printf("FZ: %d\n", Control.fz);
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
		//printf("addr is %ld, x/m d is %ld, x/m fmem is %d, m/w d is %ld, and m/w fwb is %d\n", addr, EXtoMEM.dnum, EXtoMEM.fmem, MEMtoWB.dnum, MEMtoWB.fwb);
                if ((EXtoMEM.dnum == addr) && !(isSturBranch(addr)) && (EXtoMEM.fmem == 0)) {
			//printf("x/m hit, returning %ld\n", EXtoMEM.res);
                        return EXtoMEM.res;
                }
                else if ((MEMtoWB.dnum == addr) && !(isSturBranch(addr)) && (MEMtoWB.fwb == 1)) {
			//printf("m/w hit, returning %ld\n", MEMtoWB.res);
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
        //printf("LDUR\n");
        LDUR();
        break;
      case 0xb8400000:
        //printf("LDUR\n");
        LDUR2();
        break;
      case 0x38400000:
        //printf("LDURB\n");
        LDURB();
        break;
      case 0x78400000:
        //printf("LDURH\n");
        LDURH();
        break;
      case 0xf8000000:
        //printf("STUR\n");
        STUR();
        break;
      case 0xb8000000:
        //printf("STUR\n");
        STUR2();
        break;
      case 0x38000000:
        //printf("STURB\n");
        STURB();
        break;
      case 0x78000000:
        //printf("STURH\n");
        STURH();
        break;
    }
    MEMtoWB.dnum = EXtoMEM.dnum;
    MEMtoWB.fn = EXtoMEM.fn;
    MEMtoWB.fz = EXtoMEM.fz;
    MEMtoWB.fmem = EXtoMEM.fmem;
  } else {
    MEMtoWB = (MEMtoWB_t){ .op = EXtoMEM.op, .dnum = EXtoMEM.dnum, .res = EXtoMEM.res, .fwb = EXtoMEM.fwb, .fn = EXtoMEM.fn, .fz = EXtoMEM.fz, .branching = EXtoMEM.branching};
  }
  EXtoMEM = (EXtoMEM_t){ .n = 0, .dnum = 0, .dval = 0, .imm1 = 0, .res = 0, .fmem = 0, .fn = 0, .fz = 0};
}

void pipe_stage_execute()
{
  // we have modify implementations to write to EXtoMEM.res
  if (!(IDtoEX.fmem)) {
    switch(IDtoEX.op)
    {
        // Add/Subtract immediate
        case 0x91000000:
          //printf("ADD\n");
          ADD_Immediate();
          break;
        case 0xb1000000:
          //printf("ADDS\n");
          ADDS_Immediate();
          break;
        case 0xd1000000:
          //printf("SUB\n");
          SUB_Immediate();
          break;
        case 0xf1000000:
          //printf("SUBS\n");
          SUBS_Immediate();
          break;
        // Compare and branch
        case 0xb4000000:
          //printf("CBZ\n");
          CBZ();
          break;
        case 0xb5000000:
          //printf("CBNZ\n");
          CBNZ();
          break;
        // Move wide
        case 0xd2800000:
          //printf("MOVZ\n");
          MOVZ();
          break;
        // Bitfield
        case 0xd3000000:
          //printf("LSL or LSR\n"); //execution has to do the distinction
          BITSHIFT();
          break;
        // Conditional branch
        case 0x54000000:
          //printf("B.cond\n");
          B_Cond();
          break;
        // Exceptions
        case 0xd4400000:
          //printf("HLT\n");
          HLT();
          break;
        // Unconditional branch (register)
        case 0xd61f0000:
          //printf("BR\n");
          BR();
          break;
        // Unconditional branch (immediate)
        case 0x14000000:
          //printf("B\n");
          B();
          break;

        // Logical (shifted register)
        case 0x8a000000:
          //printf("AND\n");
          AND();
          break;
        case 0xea000000:
          //printf("ANDS\n");
          ANDS();
          break;
        case 0xca000000:
          //printf("EOR\n");
          EOR();
          break;
        case 0xaa000000:
          //printf("ORR\n");
          ORR();
          break;
        // Add/subtract (extended)
        case 0x8b000000:
          //printf("ADD\n");
          ADD_Extended();
          break;
        case 0xab000000:
          //printf("ADDS\n");
          ADDS_Extended();
          break;
        case 0xcb000000:
          //printf("SUB\n");
          SUB_Extended();
          break;
        case 0xeb000000:
          //printf("SUBS\n");
          SUBS_Extended();
          break;
        // Data Processing (3 source)
        case 0x9b000000:
          //printf("MUL\n");
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
  //branch bubbling
  // if ((int)stat_cycles <= Control.branch_bubble_until) {
  //   return;
  // }
  //loadstore bubbling detection
  if (EXtoMEM.op == 0xf8400000)
  {
    loadstore_dependency = 1;
  }
  // printf("loadstore_dependency: %d\n", loadstore_dependency);
  //in loadstore bubble rn
  if ((int)stat_cycles < Control.loadstore_bubble_until)
  {
    return;
  }
  //loadstore bubbling restore to pipeline
  if ((int) stat_cycles == Control.loadstore_bubble_until)
  {
    // printf("restoration\n");
    Control.loadstore_bubble_until = -1;
    Control.restoration = 1;
    IDtoEX = (IDtoEX_t){.op = temp_IDtoEX.op, .m = temp_IDtoEX.m, .n = temp_IDtoEX.n, .dnum = temp_IDtoEX.dnum, .imm1 = temp_IDtoEX.imm1, .imm2 = temp_IDtoEX.imm2, .addr = temp_IDtoEX.addr, .fmem = temp_IDtoEX.fmem, .fwb = temp_IDtoEX.fwb};
    IDtoEX.n = reg_call(IDtoEX.n);
    IDtoEX.m = reg_call(IDtoEX.m);
    IFtoID = (IFtoID_t){.inst = temp_IFtoID.inst};
    //printf("temp_IFtoID.inst: %x\n", temp_IFtoID.inst);
    //printf("IFtoID.inst: %x\n", IFtoID.inst);
    return;
  }

  uint32_t word = IFtoID.inst;
  // printf("DECODE WORD: %x\n", word);
  int temp = word & 0x1E000000;
  // Data Processing - Immediate
  if ((temp == 0x10000000) || (temp == 0x12000000)) {
    // printf("Data Processing - Immediate, ");
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
          //printf("IDtoEX.n: %ld", IDtoEX.n);
          // printf("bubble trigger");
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
      // printf("Add/subtraction (immediate), ");
    }
    // Move wide (immediate)
    else if (temp2 == 0x02800000) {
      IDtoEX.op = (word & 0xff800000);
      IDtoEX.imm1 = (word & 0x001fffe0) >> 5;
      IDtoEX.dnum = (word & 0x0000001f);
      IDtoEX.fwb = 1;
      // printf("Move wide (immediate), ");
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
          //printf("IDtoEX.n: %ld", IDtoEX.n);
          // printf("bubble trigger\n");
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
      // printf("Bitfield, ");
    }
    else {
      //printf("Failure to match subtype0\n");
    }
  }

  // Branches and Exceptions
  else if ((temp == 0x14000000) || (temp == 0x16000000)) {
    //printf("Branches and Exceptions, ");
    // Conditional branch
    if ((word & 0xfe000000) == 0x54000000) {
      IDtoEX.op = (word & 0xff000000);
      //check d for cond
      IDtoEX.dnum = (word & 0x0000000f);
      //IDtoEX.imm = (word & 0x00ffffe0) >> 5;
      IDtoEX.addr = ((word & 0x00FFFFE0) | ((word & 0x800000) ? 0xFFFFFFFFFFF80000 : 0));
      IDtoEX.branching = 1;
      // printf("CONDITIONAL BRANCH\n");
      // TriggerBubble_Branch((int) stat_cycles + 2);
      // if (Control.prediction_taken != 1)
      // {condBubble(IDtoEX.dnum);}
      //printf("Conditional branch, ");
    }
    // Exception
    else if ((word & 0xff000000) == 0xd4000000) {
      IDtoEX.op = (word & 0xffe00000);
      IDtoEX.imm1 = (word & 0x001fffe0) >> 5;
      IDtoEX.branching = 1;
      // IFtoID.pc_halt = 1;
      //printf("Exception, ");
    }
    // Unconditional branch (register)
    else if ((word & 0xfe000000) == 0xd6000000) {
      IDtoEX.op = (word & 0xfffffc00);
      //IDtoEX.n = reg_call((word & 0x000003e0) >> 5);
      IDtoEX.n = (word & 0x000003e0) >> 5;
      IDtoEX.branching = 1;
      // printf("UNCONDITIONAL BRANCH REGISTER\n");
      // if (Control.prediction_taken != 1)
      // {TriggerBubble_Branch((int) stat_cycles + 2);}
      //printf("Unconditional branch (register), ");
    }
    // Unconditional branch (immediate)
    else if ((word & 0x60000000) == 0) {
      IDtoEX.op = (word & 0xfc000000);
      //sign extending to 64 bits
      IDtoEX.addr = (word & 0x03ffffff) | ((word & 0x2000000) ? 0xFFFFFFFFFC000000 : 0);
      IDtoEX.branching = 1;
      // printf("UNCONDITIONAL BRANCH IMMEDIATE\n");
      // if (Control.prediction_taken != 1)
      // {TriggerBubble_Branch((int) stat_cycles + 2);}
      //printf("Unconditional branch (immediate), ");
    }
    // Compare and branch
    else if ((word & 0x7e000000) == 0x34000000) {
      IDtoEX.op = (word & 0xff000000);
      IDtoEX.dnum = (word & 0x0000001f);
      // IDtoEX.dval = reg_call(IDtoEX.dnum);
      IDtoEX.dval = CURRENT_STATE.REGS[(IDtoEX.dnum)];
      IDtoEX.addr = (word & 0x00ffffe0) | ((word & 0x800000) ? 0xFFFFFFFFFF000000 : 0);
      IDtoEX.branching = 1;
      // printf("COMPARE AND BRANCH\n");
      // if (Control.prediction_taken != 1)
      // compareBubble(IDtoEX.dnum);
      //printf("Compare and branch, ");
    }
    else {
      //printf("Failure to match subtype1\n");
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
      // printf("Decode Reg: %ld\n", IDtoEX.dnum);
      // printf("IDtoEX.n: %ld\n", IDtoEX.n);
      // printf("Execute Reg: %ld\n", EXtoMEM.dnum);
      if (IDtoEX.dnum == EXtoMEM.dnum)
      {
        // printf("IDtoEX.n: %ld\n", IDtoEX.n);
        // printf("bubble trigger\n");
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
    // printf("Loads and Stores, Load/store (unscaled immediate), ");
  }
  // Data Processing - Register
  else if ((word & 0x0e000000) == 0x0a000000) {
    // printf("Data Processing - Register, ");
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
          //printf("IDtoEX.n: %ld", IDtoEX.n);
          // printf("bubble trigger\n");
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
      // printf("Logical (shifted register), ");
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
          //printf("IDtoEX.n: %ld", IDtoEX.n);
          // printf("bubble trigger\n");
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
      // printf("Add/subtract (extended register), ");
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
          //printf("IDtoEX.n: %ld", IDtoEX.n);
          // printf("bubble trigger\n");
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
      // printf("Data processing (3 source), ");
    }
    else {
      //printf("Failure to match subtype2\n");
    }
  }
  else {
    //printf("Failure to match subtype3\n");
  }
  // IFtoID = (IFtoID_t){ .inst = 0};
  loadstore_dependency = 0;

}

void pipe_stage_fetch()
{
  //exception control
  if (Control.halt == 1)
  {
    return;
  }
  //lab3 bubble: one cycle halt on fetch
  if (Control.lab3_bubble == 1)
  {
    //  printf("lab3_bubble\n");
    Control.lab3_bubble = 0;
    return;
  }

  //lab2 bubble branching
  if (Control.cond_branch == 1)
  {
      CURRENT_STATE.PC = Control.baddr;
      Control.baddr = -1;
      //IFtoID.inst = mem_read_32(CURRENT_STATE.PC);
      // printf("PC: %lx\n", CURRENT_STATE.PC);
      // printf("WORD in cond branch: %x\n",IFtoID.inst);
      // CURRENT_STATE.PC = CURRENT_STATE.PC + 4;
      Control.cond_branch = 0;
      return;
  }
  //doesn't seem like we need the lab2 bubbling behavior anymore

  // if ((int) stat_cycles < Control.branch_bubble_until)
  // {
  //   if (Control.branch_grab == 0)
  //   {
  //     printf("PC in bubble: %lx\n", CURRENT_STATE.PC);
  //     IFtoID.inst = mem_read_32(CURRENT_STATE.PC);
  //     printf("Grabbed word in bubble: %x\n", IFtoID.inst);
  //     Control.bpc = CURRENT_STATE.PC;
  //     bp_predict(CURRENT_STATE.PC);
  //     Control.branch_grab = 1;
  //     return;
  //   }
  //   else
  //   {
  //     if ((Control.baddr == (CURRENT_STATE.PC - 4)) || (Control.not_taken == 1))
  //     {
  //       IFtoID.inst = Control.squashed;
  //       printf("+4 or not_taken Restoration: %x\n", IFtoID.inst);
  //     }
  //     Control.branch_grab = 0;
  //     return;
  //   }
  // }
  // if ((stat_cycles) == Control.branch_bubble_until)
  // {
  //   // printf("ENDING BUBBLE\n");
  //   if ((Control.baddr == CURRENT_STATE.PC - 4) || (Control.not_taken))
  //   {
  //     IFtoID.inst = mem_read_32(CURRENT_STATE.PC);
  //     printf("WORD in PC + 4 Branch or Not_Taken: %x\n", IFtoID.inst);
  //     Control.not_taken = 0;
  //     Control.bpc = CURRENT_STATE.PC;
  //     bp_predict(CURRENT_STATE.PC);
  //   }
  //   else
  //   {
  //     //Control.baddr has non default value, we are branching
  //     if (Control.baddr == -1)
  //     {
  //       Control.bpc = CURRENT_STATE.PC;
  //       bp_predict(CURRENT_STATE.PC);
  //     }
  //     else
  //     {
  //       CURRENT_STATE.PC = Control.baddr;
  //       IFtoID.inst = mem_read_32(CURRENT_STATE.PC);
  //       printf("PC: %lx\n", CURRENT_STATE.PC);
  //       printf("WORD in if: %x\n",IFtoID.inst);
  //       Control.bpc = CURRENT_STATE.PC;
  //       bp_predict(CURRENT_STATE.PC);
  //     }
  //   }
  //   Control.baddr = -1;
  //   Control.branch_bubble_until = -1;
  //   return;
  // }

  //lab2 loadstore bubbling
  //loadstore bubbling stop pc
  if ((int) stat_cycles <= Control.loadstore_bubble_until)
  {
    // printf("in loadstore bubble not fetching!\n");
    Control.bpc = CURRENT_STATE.PC;
    bp_predict(CURRENT_STATE.PC);
    return;
  }
  if (Control.restoration == 1)
  {
    // printf("load store bubble restoring, not grabbing word from mem\n");
    Control.restoration = 0;
    return;
  }
  //handling branch after HLT
  if (IDtoEX.op == 0xd4400000)
  {
    CURRENT_STATE.PC = CURRENT_STATE.PC + 4;
    return;
  }

  uint32_t word = mem_read_32(CURRENT_STATE.PC);
  IFtoID.inst = word;
  // printf("WORD in general: %x\n",word);
  if (word != 0)
  {
    Control.bpc = CURRENT_STATE.PC;
    bp_predict(CURRENT_STATE.PC);
  }
  else if(Control.halt == 0)
  {
    Control.halt = 1;
    Control.bpc = CURRENT_STATE.PC;
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
  //printf("temp_IFtoTD inst: %x\n", temp_IFtoID.inst);
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
    // printf("Branch Base: %lx\n", temp);
    Control.baddr = base + (offset * 4);


    // grab then squash, will need to restore to pipeline if PC + 4
    // printf("SQUASHING in Branch: %x\n", IFtoID.inst);
    Control.squashed = IFtoID.inst;
    IFtoID = (IFtoID_t){ .inst = 0};
    
    // printf("baddr in Branch: %x\n", Control.baddr);
    return;
}
/*restore and flush function for lab3 */
void Restore_Flush(uint32_t real_target, int pred_taken, int branch_taken, uint32_t pc_before_prediction, uint64_t taken_target)
{
  // printf("pred_taken: %d\n", pred_taken);
  if (pred_taken == 0)
  {
    if ((real_target == pc_before_prediction + 4) && (branch_taken == 1))
    {
      //prediction was correct, do nothing to pipeline but remember to reset lab3 control struct fields
      // printf("prediction taken misprediction\n");
      return;
    }
    else
    {
      //misprediction on PC + 4, don't do shit
      // printf("prediction not taken misprediction\n");
      // CURRENT_STATE.PC = real_target;
      // IFtoID = (IFtoID_t){ .inst = 0};
      // Control.lab3_bubble = 1;
      //reset fields
      return;
    }
  }
  else if (pred_taken == 1)
  {
    // printf("real target: %x\n", real_target);
    // printf("taken target: %lx\n", taken_target);
    // printf("branch_taken: %d\n", branch_taken);
    // branch prediction success, don't do anything
    if ((real_target == taken_target) && (branch_taken == 1))
    {
      return;
    }
    else
    {
      // this aint working
      //misprediction, reset pc to pc before prediction, flush, and bubble
      // printf("misprediciton resolution: %x\n", pc_before_prediction);
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
    // printf("MEMtoWB.dnum: %ld\n", MEMtoWB.dnum);
    // printf("MEMtoWB.res: %ld\n", MEMtoWB.res);
    // printf("Reg value: %ld\n", CURRENT_STATE.REGS[IDtoEX.dnum]);
    int reg_load_ahead = 0;
    //seeing if function ahead is a reg load
    switch(MEMtoWB.op)
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
          //printf("SUB\n");
          reg_load_ahead = 1;
          break;
        case 0xf1000000:
          //printf("SUBS\n");
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

    if (((MEMtoWB.dnum == IDtoEX.dnum) && (MEMtoWB.res != 0) && (reg_load_ahead == 1)))
    {
      // printf("1\n");
      branch_taken = 1;
    }
    else if (((MEMtoWB.dnum == IDtoEX.dnum) && (MEMtoWB.res == 0) && (reg_load_ahead == 1)))
    {
      // printf("2\n");
      branch_taken = 0;
    }
    else if (CURRENT_STATE.REGS[IDtoEX.dnum] != 0)
    {
      // printf("3\n");
      branch_taken = 1;
    }
    else
    {
      // printf("4\n");
      branch_taken = 0;
    }

    // printf("branch_taken: %d\n", branch_taken);

    //lab2 behavior
    if (temp_entry->pred.prediction_taken == 0)
    {  
      if (branch_taken == 0)
      {
        //don't branch
        // printf("SQUASHING: %x\n", IFtoID.inst);
        // // grab then squash, will need to restore to pipeline if PC + 4
        // Control.not_taken = 1;
        // Control.squashed = IFtoID.inst;
        // IFtoID = (IFtoID_t){ .inst = 0};
      }
      else
      {
        //branch
        // printf("MEMtoWB.res = %ld\n", MEMtoWB.res);
        // printf("triggering cond_branch\n");
        Control.cond_branch = 1;
        Branch(offset, temp_entry->pred.pc_before_prediction);
      }
      //bp_update
      int inc;
      (branch_taken == 1) ? (inc = 1) : (inc = -1);
      // printf("EX func inc: %d\n", inc);
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
    // printf("MEMtoWB.op: %lx\n", MEMtoWB.op);
    switch(MEMtoWB.op)
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
    // printf("MEMtoWB.dnum: %ld\n", MEMtoWB.dnum);
    // printf("MEMtoWB.res: %ld\n", MEMtoWB.res);
    // printf("IDtoEX: %ld\n", IDtoEX.dnum);
    // printf("reg_load_ahead: %d\n", reg_load_ahead);

    if (((MEMtoWB.dnum == IDtoEX.dnum) && (MEMtoWB.res == 0) && (reg_load_ahead == 1)))
    {
      // printf("1\n");
      branch_taken = 1;
    }
    else if (((MEMtoWB.dnum == IDtoEX.dnum) && (MEMtoWB.res != 0) && (reg_load_ahead == 1)))
    {
      // printf("2\n");
      branch_taken = 0;
    }
    else if (CURRENT_STATE.REGS[IDtoEX.dnum] == 0)
    {
      // printf("3\n");
      branch_taken = 1;
    }
    else
    {
      // printf("4\n");
      branch_taken = 0;
    }
    // printf("branch_taken: %d\n", branch_taken);

    //lab2 behavior
    if (temp_entry->pred.prediction_taken == 0)
    {  
      if (branch_taken == 0)
      {
        //don't branch
        // printf("SQUASHING: %x\n", IFtoID.inst);
        // // grab then squash, will need to restore to pipeline if PC + 4
        // Control.not_taken = 1;
        // Control.squashed = IFtoID.inst;
        // IFtoID = (IFtoID_t){ .inst = 0};
      }
      else
      {
        //branch
        // printf("MEMtoWB.res = %ld\n", MEMtoWB.res);
        // printf("triggering cond_branch\n");
        Control.cond_branch = 1;
        Branch(offset, temp_entry->pred.pc_before_prediction);
      }
      //bp_update
      int inc;
      (branch_taken == 1) ? (inc = 1) : (inc = -1);
      // printf("EX func inc: %d\n", inc);
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
      // printf("SQUASHING in Branch: %x\n", IFtoID.inst);
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
    if (q.head == NULL)
		{
			// printf("EX head is null\n");
		}
		else
		{
			// printf("EX pc_before_prediction in bp_predict head: %x\n", q.head->pred.pc_before_prediction);
		}
		if (q.tail == NULL)
		{
			// printf("EX tail is null\n");
		}
		else
		{
			// printf("EX pc_before_prediction in bp_predict tail: %x\n", q.tail->pred.pc_before_prediction);
		}
    int64_t offset = IDtoEX.addr;
    struct entry* temp_entry = dequeue(&q);
    // printf("EX temp_entry pc_before_prediction: %x\n", temp_entry->pred.pc_before_prediction);
    if (temp_entry->pred.prediction_taken == 0)
    {
      Control.cond_branch = 1;
      Branch(offset, temp_entry->pred.pc_before_prediction);
      uint64_t temp = CURRENT_STATE.PC - 8;
      // printf("temp: %lx\n", temp);
      //(pc where argument was fetched, cond_bit, target addr, inc)
      // printf("branch target: %lx\n", temp + (offset * 4));
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
    // printf("B_Cond\n");
    int64_t cond = IDtoEX.dnum;
    int64_t offset = IDtoEX.addr >> 5;
    struct entry* temp_entry = dequeue(&q);
    int branch_taken;
    switch(cond)
    {
        //BEQ
        case(0):
            // printf("BEQ\n");
            // printf("offset: %ld", offset);
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
                // printf("SQUASHING: %x\n", IFtoID.inst);
                // grab then squash, will need to restore to pipeline if PC + 4
                Control.squashed = IFtoID.inst;
                IFtoID = (IFtoID_t){ .inst = 0};
              }
              //bp update and flush
              int inc;
              (branch_taken == 1) ? (inc = 1) : (inc = -1);
              // printf("EX func inc: %d\n", inc);
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
            // printf("BNE\n");

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
                // printf("SQUASHING: %x\n", IFtoID.inst);
                // grab then squash, will need to restore to pipeline if PC + 4
                Control.squashed = IFtoID.inst;
                IFtoID = (IFtoID_t){ .inst = 0};
              }
              //bp update and flush
              int inc;
              (branch_taken == 1) ? (inc = 1) : (inc = -1);
              // printf("EX func inc: %d\n", inc);
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
            // printf("BGE\n");
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
                // printf("SQUASHING: %x\n", IFtoID.inst);
                // grab then squash, will need to restore to pipeline if PC + 4
                Control.squashed = IFtoID.inst;
                IFtoID = (IFtoID_t){ .inst = 0};
              }
              //bp update and flush
              int inc;
              (branch_taken == 1) ? (inc = 1) : (inc = -1);
              // printf("EX func inc: %d\n", inc);
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
            // printf("BLT\n");
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
                // printf("SQUASHING: %x\n", IFtoID.inst);
                // grab then squash, will need to restore to pipeline if PC + 4
                Control.squashed = IFtoID.inst;
                IFtoID = (IFtoID_t){ .inst = 0};
              }
              //bp update and flush
              int inc;
              (branch_taken == 1) ? (inc = 1) : (inc = -1);
              // printf("EX func inc: %d\n", inc);
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
            // printf("temp_entry->pred.prediction_taken: %d\n", temp_entry->pred.prediction_taken);
            // printf("BGT\n");
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
                // printf("SQUASHING: %x\n", IFtoID.inst);
                // grab then squash, will need to restore to pipeline if PC + 4
                Control.squashed = IFtoID.inst;
                IFtoID = (IFtoID_t){ .inst = 0};
              }
              //bp update and flush
              int inc;
              (branch_taken == 1) ? (inc = 1) : (inc = -1);
              // printf("EX func inc: %d\n", inc);
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
            // printf("BLE\n");
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
                // printf("SQUASHING: %x\n", IFtoID.inst);
                // grab then squash, will need to restore to pipeline if PC + 4
                Control.squashed = IFtoID.inst;
                IFtoID = (IFtoID_t){ .inst = 0};
              }
              //bp update and flush
              int inc;
              (branch_taken == 1) ? (inc = 1) : (inc = -1);
              // printf("EX func inc: %d\n", inc);
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
int64_t SIGNEXTEND(int64_t offset) {
  if (offset & 0x0000000000000100) {
    offset = (offset | 0xffffffffffffff00);
  }
  return offset;
}
void LDUR() {
  int64_t t = EXtoMEM.dnum;
  int64_t n = EXtoMEM.n;
  int64_t offset = SIGNEXTEND(EXtoMEM.imm1);
  // printf("mem_loc base: %lx\n", n);
  // printf("mem_loc offset: %lx\n", offset);
  int64_t load = mem_read_32(n + offset);
  int64_t load2 = mem_read_32(n + offset + 4);
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
  int64_t load = mem_read_32(n + offset);
  if (t != 31) {
    MEMtoWB.res = load;
  }
  MEMtoWB.fwb = 1;

}
void LDURB() {
  int64_t t = EXtoMEM.dnum;
  int64_t n = EXtoMEM.n;
  int64_t offset = SIGNEXTEND(EXtoMEM.imm1);
  int load = mem_read_32(n + offset);
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
  int load = mem_read_32(n + offset);
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
  mem_write_32(n + offset, (t & 0x00000000ffffffff));
  mem_write_32(n + offset + 4, ((t & 0xffffffff00000000) >> 32));
  MEMtoWB.fwb = 0;

}
void STUR2() {
  int64_t t = EXtoMEM.dval;
  int64_t n = EXtoMEM.n;
  int64_t offset = SIGNEXTEND(EXtoMEM.imm1);
  mem_write_32(n + offset, t);
  MEMtoWB.fwb = 0;

}
void STURB() {
  char t = EXtoMEM.dval;
  int64_t n = EXtoMEM.n;
  int64_t offset = SIGNEXTEND(EXtoMEM.imm1);
  int load = mem_read_32(n + offset);
  load = (load & 0xffffff00) | (int)t;
  mem_write_32(n + offset, load);
  MEMtoWB.fwb = 0;

}
void STURH() {
  int t = EXtoMEM.dval;
  int64_t n = EXtoMEM.n;
  int64_t offset = SIGNEXTEND(EXtoMEM.imm1);
  int load = mem_read_32(n + offset);
  // printf("first load: %x\n", (load & 0xffffff00));
  // printf("second load: %x\n", (t & 0x0000ffff));
  //original implementation
  // load = (load & 0xffffff00) | (t & 0x0000ffff);
  load = t & 0x0000ffff;
  // printf("STURH load: %x\n", load);
  mem_write_32(n + offset, load);
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