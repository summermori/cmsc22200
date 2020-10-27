/*
 * CMSC 22200
 *
 * ARM pipeline timing simulator
 */

#include "pipe.h"
#include "shell.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>



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


void pipe_cycle()
{
	pipe_stage_wb();
	pipe_stage_mem();
	pipe_stage_execute();
	pipe_stage_decode();
	pipe_stage_fetch();
  RUN_BIT = false;
}



void pipe_stage_wb()
{
  if ((MEMtoWB.dnum != 31) && (MEMtoWB.fwb)){
    CURRENT_STATE.REGS[MEMtoWB.dnum] = MEMtoWB.res;
  }
  CURRENT_STATE.FLAG_Z = MEMtoWB.fz;
  CURRENT_STATE.FLAG_N = MEMtoWB.fn;
  MEMtoWB = (MEMtoWB_t){.dnum = 0, .fwb = 0, .res = 0, .fn = 0, .fz = 0};
}

void pipe_stage_mem()
{
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
  } else {
    MEMtoWB = (MEMtoWB_t){ .dnum = EXtoMEM.dnum, .res = EXtoMEM.res, .fwb = EXtoMEM.fwb, .fn = EXtoMEM.fn, .fz = EXtoMEM.fz};
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
    //     case 0xd1000000:
    //       //printf("SUB\n");
    //       SUB_Immediate();
    //       break;
    //     case 0xf1000000:
    //       //printf("SUBS\n");
    //       SUBS_Immediate();
    //       break;
    //     // Compare and branch
    //     case 0xb4000000:
    //       //printf("CBZ\n");
    //       CBZ();
    //       break;
    //     case 0xb5000000:
    //       //printf("CBNZ\n");
    //       CBNZ();
    //       break;
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
    //     // Conditional branch
    //     case 0x54000000:
    //       //printf("B.cond\n");
    //       B_Cond();
    //       break;
    //     // Exceptions
    //     case 0xd4400000:
    //       //printf("HLT\n");
    //       HLT();
    //       break;
    //     // Unconditional branch (register)
    //     case 0xd61f0000:
    //       //printf("BR\n");
    //       BR();
    //       break;
    //     // Unconditional branch (immediate)
    //     case 0x14000000:
    //       //printf("B\n");
    //       B();
    //       break;
    //
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
    //     case 0xcb000000:
    //       //printf("SUB\n");
    //       SUB_Extended();
    //       break;
    //     case 0xeb000000:
    //       //printf("SUBS\n");
    //       SUBS_Extended();
    //       break;
    //     // Data Processing (3 source)
    //     case 0x9b000000:
    //       //printf("MUL\n");
    //       MUL();
    //       break;
    }
  } else {
    EXtoMEM.op = IDtoEX.op;
    EXtoMEM.n = IDtoEX.n;
    EXtoMEM.dval = IDtoEX.dval;
    EXtoMEM.imm1 = IDtoEX.imm1;
  }
  EXtoMEM.dnum = IDtoEX.dnum;
  EXtoMEM.fwb = IDtoEX.fwb;
  EXtoMEM.fmem = IDtoEX.fmem;
  IDtoEX = (IDtoEX_t){ .op = 0, .m = 0, .n = 0, .dnum = 0, .imm1 = 0, .imm2 = 0, .addr = 0, .fmem = 0, .fwb = 0};
}

void pipe_stage_decode()
{
  uint32_t word = IFtoID.inst;
  printf("%d ", word);
  int temp = word & 0x1E000000;
  // Data Processing - Immediate
  if ((temp == 0x10000000) || (temp == 0x12000000)) {
    printf("Data Processing - Immediate, ");
    int temp2 = word & 0x03800000;
    // Add/subtract (immediate)
    if ((temp2 == 0x01000000) || (temp2 == 0x01800000)) {
      IDtoEX.op = (word & 0xff000000);
      IDtoEX.imm1 = (word & 0x003ffc00) >> 10;
      IDtoEX.n = CURRENT_STATE.REGS[(word & 0x000003e0) >> 5];
      IDtoEX.dnum = (word & 0x0000001f);
      IDtoEX.fwb = 1;
      printf("Add/subtraction (immediate), ");
    }
    // Move wide (immediate)
    else if (temp2 == 0x02800000) {
      IDtoEX.op = (word & 0xff800000);
      IDtoEX.imm1 = (word & 0x001fffe0) >> 5;
      IDtoEX.dnum = (word & 0x0000001f);
      IDtoEX.fwb = 1;
      printf("Move wide (immediate), ");
    }
    // Bitfield
    else if (temp2 == 0x03000000) {
      // this should be reviewed, since i am unsure what you guys need - victor
      IDtoEX.op = (word & 0xff800000);
      IDtoEX.n = CURRENT_STATE.REGS[(word & 0x000003e0) >> 5];
      IDtoEX.dnum = (word & 0x0000001f);
      IDtoEX.imm1 = (word & 0x0000fc00) >> 10;
      IDtoEX.imm2 = (word & 0x003f0000) >> 16; // using m as another imm
      IDtoEX.fwb = 1;
      printf("Bitfield, ");
    }
    else {
      printf("Failure to match subtype0\n");
    }
  }

  // Branches and Exceptions
  else if ((temp == 0x14000000) || (temp == 0x16000000)) {
    printf("Branches and Exceptions, ");
    // Conditional branch
    if ((word & 0xfe000000) == 0x54000000) {
      IDtoEX.op = (word & 0xff000000);
      //check d for cond
      IDtoEX.dnum = (word & 0x0000000f);
      //IDtoEX.imm = (word & 0x00ffffe0) >> 5;
      IDtoEX.addr = ((word & 0x00FFFFE0) | ((word & 0x800000) ? 0xFFFFFFFFFFF80000 : 0));
      printf("Conditional branch, ");
    }
    // Exception
    else if ((word & 0xff000000) == 0xd4000000) {
      IDtoEX.op = (word & 0xffe00000);
      IDtoEX.imm1 = (word & 0x001fffe0) >> 5;
      printf("Exception, ");
    }
    // Unconditional branch (register)
    else if ((word & 0xfe000000) == 0xd6000000) {
      IDtoEX.op = (word & 0xfffffc00);
      IDtoEX.n = CURRENT_STATE.REGS[(word & 0x000003e0) >> 5];
      printf("Unconditional branch (register), ");
    }
    // Unconditional branch (immediate)
    else if ((word & 0x60000000) == 0) {
      IDtoEX.op = (word & 0xfc000000);
      //sign extending to 64 bits
      IDtoEX.addr = (word & 0x03ffffff) | ((word & 0x2000000) ? 0xFFFFFFFFFC000000 : 0);
      printf("Unconditional branch (immediate), ");
    }
    // Compare and branch
    else if ((word & 0x7e000000) == 0x34000000) {
      IDtoEX.op = (word & 0xff000000);
      IDtoEX.dnum = (word & 0x0000001f);
      IDtoEX.addr = (word & 0x00ffffe0) | ((word & 0x800000) ? 0xFFFFFFFFFF000000 : 0);
      printf("Compare and branch, ");
    }
    else {
      printf("Failure to match subtype1\n");
    }
  }
  // Loads and Stores
  else if ((word & 0x0a000000) == 0x08000000) {
    IDtoEX.op = (word & 0xffc00000);
    IDtoEX.n = CURRENT_STATE.REGS[(word & 0x000003e0) >> 5];
    IDtoEX.dnum = (word & 0x0000001f);
    IDtoEX.imm1 = (word & 0x001ff000) >> 12;
    IDtoEX.dval = CURRENT_STATE.REGS[IDtoEX.dnum];
    IDtoEX.fmem = 1;
    printf("Loads and Stores, Load/store (unscaled immediate), ");
  }
  // Data Processing - Register
  else if ((word & 0x0e000000) == 0x0a000000) {
    printf("Data Processing - Register, ");
    // Logical (shifted register)
    if ((word & 0x1f000000) == 0x0a000000) {
      IDtoEX.op = (word & 0xff000000);
      IDtoEX.m = CURRENT_STATE.REGS[(word & 0x001f0000) >> 16];
      IDtoEX.n = CURRENT_STATE.REGS[(word & 0x000003e0) >> 5];
      IDtoEX.dnum = (word & 0x0000001f);
      IDtoEX.imm1 = (word & 0x0000fc00) >> 10;
      IDtoEX.fwb = 1;
      printf("Logical (shifted register), ");
    }
    // Add/subtract (extended register)
    else if ((word & 0x1f200000) == 0x0b000000) {
      IDtoEX.op = (word & 0xffe00000);
      IDtoEX.m = CURRENT_STATE.REGS[(word & 0x001f0000) >> 16];
      IDtoEX.n = CURRENT_STATE.REGS[(word & 0x000003e0) >> 5];
      IDtoEX.dnum = (word & 0x0000001f);
      IDtoEX.fwb = 1;
      printf("Add/subtract (extended register), ");
    }
    // Data processing (3 source)
    else if ((word & 0x1f000000) == 0x1b000000) {
      IDtoEX.op = (word & 0xffe00000);
      IDtoEX.m = CURRENT_STATE.REGS[(word & 0x001f0000) >> 16];
      IDtoEX.n = CURRENT_STATE.REGS[(word & 0x000003e0) >> 5];
      IDtoEX.dnum = (word & 0x0000001f);
      IDtoEX.fwb = 1;
      printf("Data processing (3 source), ");
    }
    else {
      printf("Failure to match subtype2\n");
    }
  }
  else {
    printf("Failure to match subtype3\n");
  }
  IFtoID = (IFtoID_t){ .inst = 0};
}

void pipe_stage_fetch()
{
  uint32_t word = mem_read_32(CURRENT_STATE.PC);
  IFtoID.inst = word;
  printf("%x\n",word);
  CURRENT_STATE.PC = CURRENT_STATE.PC + 4;
}

/* instruction implementations */
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
  load = (load & 0xffffff00) | (t & 0x0000ffff);
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
        EXtoMEM.fz = 1;
        EXtoMEM.fn = 0;
    }
    else if (res < 0)
    {
        EXtoMEM.fn = 1;
        EXtoMEM.fz = 0;
    }
    else
    {
        EXtoMEM.fz = 0;
        EXtoMEM.fn = 0;
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
        EXtoMEM.fz = 1;
        EXtoMEM.fn = 0;
    }
    else if (res < 0)
    {
        EXtoMEM.fn = 1;
        EXtoMEM.fz = 0;
    }
    else
    {
        EXtoMEM.fz = 0;
        EXtoMEM.fn = 0;
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
        EXtoMEM.fz = 1;
        EXtoMEM.fn = 0;
    }
    else if (res < 0)
    {
        EXtoMEM.fn = 1;
        EXtoMEM.fz = 0;
    }
    else
    {
        EXtoMEM.fz = 0;
        EXtoMEM.fn = 0;
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
