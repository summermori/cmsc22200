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
EXtoMEM_t EXtoMEM = { .n = 0, .dnum = 0, .dval = 0, .imm1 = 0, .res = 0, .fmem = 0, .fwb = 0, .fn = 0, .fz = 0, .branching = 0};
MEMtoWB_t MEMtoWB = {.dnum = 0, .res = 0, .fwb = 0, .fn = 0, .fz = 0, .branching = 0};
Control_t Control = {.baddr = 0, .bubble_untilif = -1, .bubble_until = -1};
IDtoEX_t temp_IDtoEX;
IFtoID_t temp_IFtoID;

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
  printf("WB: %ld\n", MEMtoWB.dnum);
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
  // printf("%ld, %d, %d, %d", MEMtoWB.dnum, MEMtoWB.fwb, MEMtoWB.branching, MEMtoWB.fmem);
  if (MEMtoWB.dnum != 0 || MEMtoWB.fwb != 0 || MEMtoWB.branching != 0 || MEMtoWB.fmem != 0)
  {
    stat_inst_retire = stat_inst_retire + 1;
  }
  MEMtoWB = (MEMtoWB_t){.dnum = 0, .fwb = 0, .res = 0, .fn = 0, .fz = 0, .fmem = 0, .branching = 0};
}

void pipe_stage_mem()
{
  printf("MEM: %ld\n", EXtoMEM.dnum);
  if (EXtoMEM.halt == 1)
  {
    MEMtoWB.halt = 1;
    return;
  }
  else
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
      MEMtoWB.fmem = EXtoMEM.fmem;
    } else {
      MEMtoWB = (MEMtoWB_t){ .dnum = EXtoMEM.dnum, .res = EXtoMEM.res, .fwb = EXtoMEM.fwb, .fn = EXtoMEM.fn, .fz = EXtoMEM.fz, .branching = EXtoMEM.branching};
    }
    EXtoMEM = (EXtoMEM_t){ .n = 0, .dnum = 0, .dval = 0, .imm1 = 0, .res = 0, .fmem = 0, .fn = 0, .fz = 0};
  }
}

void pipe_stage_execute()
{
  //bubble testing
  printf("EX: %ld\n", IDtoEX.dnum);
  //updating in
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
  EXtoMEM.dnum = IDtoEX.dnum;
  EXtoMEM.fwb = IDtoEX.fwb;
  EXtoMEM.fmem = IDtoEX.fmem;
  EXtoMEM.branching = IDtoEX.branching;

  //dont clear IDtoEX if in bubble
  if ((int) stat_cycles <= Control.bubble_until)
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
  //some type of detection to trigger bubbles and forwarding
  uint32_t word = IFtoID.inst;
  //updating instruction counter
  // printf("%d ", word);
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
      IDtoEX.branching = 1;
      printf("Conditional branch, ");
    }
    // Exception
    else if ((word & 0xff000000) == 0xd4000000) {
      IDtoEX.op = (word & 0xffe00000);
      IDtoEX.imm1 = (word & 0x001fffe0) >> 5;
      IDtoEX.branching = 1;
      printf("Exception, ");
    }
    // Unconditional branch (register)
    else if ((word & 0xfe000000) == 0xd6000000) {
      IDtoEX.op = (word & 0xfffffc00);
      IDtoEX.n = CURRENT_STATE.REGS[(word & 0x000003e0) >> 5];
      IDtoEX.branching = 1;
      printf("Unconditional branch (register), ");
    }
    // Unconditional branch (immediate)
    else if ((word & 0x60000000) == 0) {
      IDtoEX.op = (word & 0xfc000000);
      //sign extending to 64 bits
      IDtoEX.addr = (word & 0x03ffffff) | ((word & 0x2000000) ? 0xFFFFFFFFFC000000 : 0);
      IDtoEX.branching = 1;
      printf("Unconditional branch (immediate), ");
    }
    // Compare and branch
    else if ((word & 0x7e000000) == 0x34000000) {
      IDtoEX.op = (word & 0xff000000);
      IDtoEX.dnum = (word & 0x0000001f);
      IDtoEX.dval = CURRENT_STATE.REGS[IDtoEX.dnum];
      IDtoEX.addr = (word & 0x00ffffe0) | ((word & 0x800000) ? 0xFFFFFFFFFF000000 : 0);
      IDtoEX.branching = 1;
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
  //commenting out for pc_halt
  // IFtoID = (IFtoID_t){ .inst = 0};

  //checking if we are in bubble, if last cycle of bubble, restore structs
  if ((int)stat_cycles < Control.bubble_until)
  {
    return;
  }
  if ((int) stat_cycles == Control.bubble_until)
  {
    printf("bubble ended\n");
    Control.bubble_until = -1;
    //continue propogation by restoring previous structs to pipeline
    IDtoEX = (IDtoEX_t){.op = temp_IDtoEX.op, .m = temp_IDtoEX.m, .n = temp_IDtoEX.n, .dnum = temp_IDtoEX.dnum, .imm1 = temp_IDtoEX.imm1, .imm2 = temp_IDtoEX.imm2, .addr = temp_IDtoEX.addr, .fmem = temp_IDtoEX.fmem, .fwb = temp_IDtoEX.fwb};
    IFtoID = (IFtoID_t){.inst = temp_IFtoID.inst};
    // printf("dnum: %ld\n", IDtoEX.dnum);
  }

  //triggering bubble on cycle 2 to test
  if ((int) stat_cycles == 2)
  {
    TriggerBubble(8);
  }
}

void pipe_stage_fetch()
{
  //dont move PC if we are bubbling
  // < or <= ?
  if ((int) stat_cycles <= Control.bubble_until)
  {
    // printf("%d, %d", stat_cycles, Control.bubble_until);
    return;
  }
  uint32_t word = mem_read_32(CURRENT_STATE.PC);
  IFtoID.inst = word;
  printf("word:%x\n",word);
  // don't move PC if we have hit an HLT();
  if (word != 0)
  {
    CURRENT_STATE.PC = CURRENT_STATE.PC + 4;
  }
  else if(IFtoID.pc_halt != 1)
  {
    IFtoID.pc_halt = 1;
    CURRENT_STATE.PC = CURRENT_STATE.PC + 4;
  }
}
/* bubble implementations */
void TriggerBubble(int bubble_until)
{
  Control.bubble_until = bubble_until;
  temp_IDtoEX = (IDtoEX_t){.op = IDtoEX.op, .m = IDtoEX.m, .n = IDtoEX.n, .dnum = IDtoEX.dnum, .imm1 = IDtoEX.imm1, .imm2 = IDtoEX.imm2, .addr = IDtoEX.addr, .fmem = IDtoEX.fmem, .fwb = IDtoEX.fwb};
  IDtoEX = (IDtoEX_t){ .op = 0, .m = 0, .n = 0, .dnum = 0, .imm1 = 0, .imm2 = 0, .addr = 0, .fmem = 0, .fwb = 0};
  temp_IFtoID = (IFtoID_t){.inst = IFtoID.inst};
  IFtoID = (IFtoID_t){ .inst = 0};
  return;
}

/* instruction implementations */
// branching helper
void Branch(int64_t offset)
{
    //we're gonna need a branching field somewhere to tell fetch() not to increment PC by 4.
    // Decode_State.branching = 1;
    uint64_t temp = CURRENT_STATE.PC;
    //dont think I need to cast here, might be wrong tho
    CURRENT_STATE.PC = temp + (offset * 4);
    return;
}
void CBNZ()
{
    //int64_t t = IDtoEX.dnum;
    int64_t offset = IDtoEX.addr/32;
    if (IDtoEX.dval != 0)
    {
      Branch(offset);
    }
    //set fields to tell MEM and WB there shouldn't do anything cause its a branching op
    return;
}
void CBZ()
{
    //int64_t t = IDtoEX.dnum;
    int64_t offset = IDtoEX.addr/32;
    if (IDtoEX.dval == 0)
    {
      Branch(offset);
    }
    //set fields to tell MEM and WB they shouldn't do anything cause its a branching op
    return;
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
    return;
}
void BR()
{
    CURRENT_STATE.PC = IDtoEX.n;
    // Decode_State.branching = 1;
    //set the branching field and fields for MEM and WB saying we dont need to do anything.
    return;
}
void B()
{
    int64_t target = IDtoEX.addr;
    Branch(target);
    return;
}
void B_Cond()
{
    int64_t cond = IDtoEX.dnum;
    int64_t offset = IDtoEX.addr >> 5;
    switch(cond)
    {
        //BEQ
        case(0):
            if (CURRENT_STATE.FLAG_Z == 1)
            {Branch(offset);}
            break;
        //BNE
        case(1):
            if (CURRENT_STATE.FLAG_Z == 0)
            {Branch(offset);}
            break;
        //BGE
        case(10):
            if ((CURRENT_STATE.FLAG_Z == 1) || (CURRENT_STATE.FLAG_N == 0))
            {Branch(offset);}
            break;
        //BLT
        case(11):
            if ((CURRENT_STATE.FLAG_N == 1) && (CURRENT_STATE.FLAG_Z == 0))
            {Branch(offset);}
            break;
        //BGT
        case(12):
            if ((CURRENT_STATE.FLAG_N == 0) && (CURRENT_STATE.FLAG_Z == 0))
            {Branch(offset);}
            break;
        //BLE
        case(13):
            if ((CURRENT_STATE.FLAG_Z == 1) || (CURRENT_STATE.FLAG_N == 1))
            {Branch(offset);}
            break;
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
        EXtoMEM.fn = 0;
        EXtoMEM.fz = 0;
    }
    EXtoMEM.res = res;
}
void SUBS_Extended()
{
    int64_t res = IDtoEX.n - IDtoEX.m;
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
        EXtoMEM.fn = 0;
        EXtoMEM.fz = 0;
    }
    EXtoMEM.res = res;
}
