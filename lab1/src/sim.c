#include <stdio.h>
#include <stdlib.h>
#include "shell.h"

typedef struct Decodes {
    int op;
    int64_t d; //reg address
    int64_t n; //reg address
    int64_t m; //reg address
    int64_t t; //reg address for branching ops
    int64_t branch_to; //potentially branch to address
    //for branch instructions we set NEXT.pc to where we wanna branch, so at the end of execute we don't wanna increment NEXT.pc by four
    int branching;
    int64_t imm;
} Decodes;

struct Decodes Decode_State = { .op = 0, .d = 0, .n = 0, .m = 0, .t = 0, .branch_to = 0, .branching = 0, .imm = 0 };

void fetch()
{
    uint32_t word = mem_read_32(CURRENT_STATE.PC);
    //printf("%x\n",word);
    NEXT_STATE.PC = word;
}
void decode()
{
  int word = NEXT_STATE.PC;
  int temp = word & 0x1E000000;
  // Data Processing - Immediate
  if ((temp == 0x10000000) || (temp == 0x12000000)) {
    //printf("Data Processing - Immediate, ");
    int temp2 = word & 0x03800000;
    // Add/subtract (immediate)
    if ((temp2 == 0x01000000) || (temp2 == 0x01800000)) {
      Decode_State.op = (word & 0xff000000);
      Decode_State.imm = (word & 0x003ffc00) >> 10;
      Decode_State.n = (word & 0x000003e0) >> 5;
      Decode_State.d = (word & 0x0000001f);
      //printf("Add/subtraction (immediate), ");
    }
    // Move wide (immediate)
    else if (temp2 == 0x02800000) {
      Decode_State.op = (word & 0xff800000);
      Decode_State.imm = (word & 0x001fffe0) >> 5;
      Decode_State.d = (word & 0x0000001f);
      //printf("Move wide (immediate), ");
    }
    // Bitfield
    else if (temp2 == 0x03000000) {
      // this should be reviewed, since i am unsure what you guys need - victor
      Decode_State.op = (word & 0xff800000);
      Decode_State.n = (word & 0x000003e0) >> 5;
      Decode_State.d = (word & 0x0000001f);
      Decode_State.imm = (word & 0x0000fc00) >> 10;
      Decode_State.m = (word & 0x003f0000) >> 16; // using m as another imm
      //printf("Bitfield, ");
    }
    else {
      //printf("Failure to match subtype");
    }
  }

  // Branches and Exceptions
  else if ((temp == 0x14000000) || (temp == 0x16000000)) {
    //printf("Branches and Exceptions, ");
    // Conditional branch
    if ((word & 0xfe000000) == 0x54000000) {
      Decode_State.op = (word & 0xff000000);
      //check d for cond
      Decode_State.d = (word & 0x0000000f);
      //Decode_State.imm = (word & 0x00ffffe0) >> 5;
      Decode_State.imm = ((word & 0x00FFFFE0) | ((word & 0x800000) ? 0xFFFFFFFFFFF80000 : 0));
      //printf("Conditional branch, ");
    }
    // Exception
    else if ((word & 0xff000000) == 0xd4000000) {
      Decode_State.op = (word & 0xffe00000);
      Decode_State.imm = (word & 0x001fffe0) >> 5;
      //printf("Exception, ");
    }
    // Unconditional branch (register)
    else if ((word & 0xfe000000) == 0xd6000000) {
      Decode_State.op = (word & 0xfffffc00);
      Decode_State.n = (word & 0x000003e0) >> 5;
      //printf("Unconditional branch (register), ");
    }
    // Unconditional branch (immediate)
    else if ((word & 0x60000000) == 0) {
      Decode_State.op = (word & 0xfc000000);
      //sign extending to 64 bits
      Decode_State.imm = (word & 0x03ffffff) | ((word & 0x2000000) ? 0xFFFFFFFFFC000000 : 0);
      //printf("Unconditional branch (immediate), ");
    }
    // Compare and branch
    else if ((word & 0x7e000000) == 0x34000000) {
      Decode_State.op = (word & 0xff000000);
      Decode_State.t = (word & 0x0000001f);
      Decode_State.imm = (word & 0x00ffffe0) | ((word & 0x800000) ? 0xFFFFFFFFFF000000 : 0);
      //printf("Compare and branch, ");
    }
    else {
      //printf("Failure to match subtype");
    }
  }
  // Loads and Stores
  else if ((word & 0x0a000000) == 0x08000000) {
    Decode_State.op = (word & 0xffc00000);
    Decode_State.n = (word & 0x000003e0) >> 5;
    Decode_State.d = (word & 0x0000001f);
    Decode_State.imm = (word & 0x001ff000) >> 12;
    Decode_State.m = word;
    //printf("Loads and Stores, Load/store (unscaled immediate), ");
  }
  // Data Processing - Register
  else if ((word & 0x0e000000) == 0x0a000000) {
    //printf("Data Processing - Register, ");
    // Logical (shifted register)
    if ((word & 0x1f000000) == 0x0a000000) {
      Decode_State.op = (word & 0xff000000);
      Decode_State.m = (word & 0x001f0000) >> 16;
      Decode_State.n = (word & 0x000003e0) >> 5;
      Decode_State.d = (word & 0x0000001f);
      Decode_State.imm = (word & 0x0000fc00) >> 10;
      // Decode_State.shift = (word & 0x00c00000) >> 22; // i don't know how you guys would like me to store this - victor
      //printf("Logical (shifted register), ");
    }
    // Add/subtract (extended register)
    else if ((word & 0x1f200000) == 0x0b000000) {
      Decode_State.op = (word & 0xffe00000);
      Decode_State.m = (word & 0x001f0000) >> 16;
      Decode_State.n = (word & 0x000003e0) >> 5;
      Decode_State.d = (word & 0x0000001f);
      //printf("Add/subtract (extended register), ");
    }
    // Data processing (3 source)
    else if ((word & 0x1f000000) == 0x1b000000) {
      Decode_State.op = (word & 0xffe00000);
      Decode_State.m = (word & 0x001f0000) >> 16;
      Decode_State.n = (word & 0x000003e0) >> 5;
      Decode_State.d = (word & 0x0000001f);
      //printf("Data processing (3 source), ");
    }
    else {
      //printf("Failure to match subtype");
    }
  }
  else {
    //printf("Failure to match subtype");
  }
}

// instruction functions
//helper functions
//helper function for branching ops
void Branch(int64_t offset)
{
    Decode_State.branching = 1;
    NEXT_STATE.PC = CURRENT_STATE.PC + (offset * 4);
    return;
}
//instruction set
void ADD_Extended()
{
    int64_t d = Decode_State.d;
    int64_t n = NEXT_STATE.REGS[Decode_State.n];
    int64_t m = NEXT_STATE.REGS[Decode_State.m];
    int64_t res = n + m;
    if (d != 31) {
      NEXT_STATE.REGS[d] = res;
    }
    return;

}
void ADD_Immediate()
{
    int64_t d = Decode_State.d;
    int64_t n = NEXT_STATE.REGS[Decode_State.n];
    int64_t imm = Decode_State.imm;
    int64_t res = n + imm;
    if (d != 31) {
      NEXT_STATE.REGS[d] = res;
    }
    return;

}
void ADDS_Extended()
{
    int64_t d = Decode_State.d;
    int64_t n = NEXT_STATE.REGS[Decode_State.n];
    int64_t m = NEXT_STATE.REGS[Decode_State.m];
    int64_t res =  n + m;
    if (res == 0)
    {
        NEXT_STATE.FLAG_Z = 1;
        NEXT_STATE.FLAG_N = 0;
    }
    else if (res < 0)
    {
        NEXT_STATE.FLAG_N = 1;
        NEXT_STATE.FLAG_Z = 0;
    }
    else
    {
        NEXT_STATE.FLAG_Z = 0;
        NEXT_STATE.FLAG_N = 0;
    }
    if (d != 31) {
      NEXT_STATE.REGS[d] = res;
    }
    return;
}
void ADDS_Immediate()
{
    int64_t d = Decode_State.d;
    int64_t n = NEXT_STATE.REGS[Decode_State.n];
    int64_t imm = Decode_State.imm;
    int64_t res = n + imm;
    if (res == 0)
    {
        NEXT_STATE.FLAG_Z = 1;
        NEXT_STATE.FLAG_N = 0;
    }
    else if (res < 0)
    {
        NEXT_STATE.FLAG_N = 1;
        NEXT_STATE.FLAG_Z = 0;
    }
    else
    {
        NEXT_STATE.FLAG_Z = 0;
        NEXT_STATE.FLAG_N = 0;
    }
    if (d != 31) {
      NEXT_STATE.REGS[d] = res;
    }
    return;
}
//register to check
void CBNZ()
{
    int64_t t = Decode_State.t;
    int64_t offset = Decode_State.imm/32;
    if (NEXT_STATE.REGS[t] != 0)
    {
      Branch(offset);
    }
    return;
}
void CBZ()
{
    int64_t t = Decode_State.t;
    int64_t offset = Decode_State.imm/32;
    if (NEXT_STATE.REGS[t] == 0)
    {
      Branch(offset);
    }
    return;
}
void AND()
{
    int64_t d = Decode_State.d;
    int64_t n = Decode_State.n;
    int64_t m = Decode_State.m;
    if (d != 31) {
      NEXT_STATE.REGS[d] = NEXT_STATE.REGS[n] & NEXT_STATE.REGS[m];
    }
    return;
}
void ANDS()
{
    int64_t d = Decode_State.d;
    int64_t n = Decode_State.n;
    int64_t m = Decode_State.m;
    int64_t res = NEXT_STATE.REGS[n] & NEXT_STATE.REGS[m];
    if (res == 0)
    {
        NEXT_STATE.FLAG_Z = 1;
        NEXT_STATE.FLAG_N = 0;
    }
    else if (res < 0)
    {
        NEXT_STATE.FLAG_N = 1;
        NEXT_STATE.FLAG_Z = 0;
    }
    else
    {
        NEXT_STATE.FLAG_Z = 0;
        NEXT_STATE.FLAG_N = 0;
    }
    if (d != 31) {
      NEXT_STATE.REGS[d] = res;
    }
    return;
}
void EOR()
{
    int64_t d = Decode_State.d;
    int64_t n = Decode_State.n;
    int64_t m = Decode_State.m;
    if (d != 31) {
      NEXT_STATE.REGS[d] = NEXT_STATE.REGS[n] ^ NEXT_STATE.REGS[m];
    }
    return;
}
void ORR()
{
    int64_t d = Decode_State.d;
    int64_t n = Decode_State.n;
    int64_t m = Decode_State.m;
    if (d != 31) {
      NEXT_STATE.REGS[d] = NEXT_STATE.REGS[n] | NEXT_STATE.REGS[m];
    }
    return;
}
void BITSHIFT()
{
  int64_t d = Decode_State.d;
  int64_t n = NEXT_STATE.REGS[Decode_State.n];
  int64_t immr = Decode_State.m;
  int64_t res;
  if (Decode_State.imm == 63) { // LSR
    res = n >> immr;
  }
  else { // LSL
    res = n << (64 - immr);
  }
  if (d != 31) {
    NEXT_STATE.REGS[d] = res;
  }
  return;
}
int64_t SIGNEXTEND(int64_t offset) {
  if (offset & 0x0000000000000100) {
    offset = (offset | 0xffffffffffffff00);
  }
  return offset;
}
void LDUR() {
  int64_t t = Decode_State.d;
  int64_t n = NEXT_STATE.REGS[Decode_State.n];
  int64_t offset = SIGNEXTEND(Decode_State.imm);
  int64_t load = mem_read_32(n + offset);
  int64_t load2 = mem_read_32(n + offset + 4);
  load = load | (load2 << 32);
  if (t != 31) {
    NEXT_STATE.REGS[t] = load;
  }
}
void LDUR2() {
  int64_t t = Decode_State.d;
  int64_t n = NEXT_STATE.REGS[Decode_State.n];
  int64_t offset = SIGNEXTEND(Decode_State.imm);
  int64_t load = mem_read_32(n + offset);
  if (t != 31) {
    NEXT_STATE.REGS[t] = load;
  }
}
void LDURB() {
  int64_t t = Decode_State.d;
  int64_t n = NEXT_STATE.REGS[Decode_State.n];
  int64_t offset = SIGNEXTEND(Decode_State.imm);
  int load = mem_read_32(n + offset);
  load = (load & 0x000000ff);
  if (t != 31) {
    NEXT_STATE.REGS[t] = load;
  }
}
void LDURH() {
  int64_t t = Decode_State.d;
  int64_t n = NEXT_STATE.REGS[Decode_State.n];
  int64_t offset = SIGNEXTEND(Decode_State.imm);
  int load = mem_read_32(n + offset);
  load = (load & 0x0000ffff);
  if (t != 31) {
    NEXT_STATE.REGS[t] = load;
  }
}
void STUR() {
  int64_t t = NEXT_STATE.REGS[Decode_State.d];
  int64_t n = NEXT_STATE.REGS[Decode_State.n];
  int64_t offset = SIGNEXTEND(Decode_State.imm);
  mem_write_32(n + offset, (t & 0x00000000ffffffff));
  mem_write_32(n + offset + 4, ((t & 0xffffffff00000000) >> 32));
}
void STUR2() {
  int64_t t = NEXT_STATE.REGS[Decode_State.d];
  int64_t n = NEXT_STATE.REGS[Decode_State.n];
  int64_t offset = SIGNEXTEND(Decode_State.imm);
  mem_write_32(n + offset, t);
}
void STURB() {
  char t = NEXT_STATE.REGS[Decode_State.d];
  int64_t n = NEXT_STATE.REGS[Decode_State.n];
  int64_t offset = SIGNEXTEND(Decode_State.imm);
  int load = mem_read_32(n + offset);
  load = (load & 0xffffff00) | (int)t;
  mem_write_32(n + offset, load);
}
void STURH() { // need to revise the size of what is being written
  int t = NEXT_STATE.REGS[Decode_State.d];
  int64_t n = NEXT_STATE.REGS[Decode_State.n];
  int64_t offset = SIGNEXTEND(Decode_State.imm);
  int load = mem_read_32(n + offset);
  load = (load & 0xffffff00) | (t & 0x0000ffff);
  mem_write_32(n + offset, load);
}
void SUB_Immediate()
{
    int64_t d = Decode_State.d;
    int64_t n = Decode_State.n;
    int64_t imm = Decode_State.imm;
    int64_t res = NEXT_STATE.REGS[n] - imm;
    if (d != 31)
    {
      NEXT_STATE.REGS[d] = res;
    }
    return;

}
void SUB_Extended()
{
    int64_t d = Decode_State.d;
    int64_t n = Decode_State.n;
    int64_t m = Decode_State.m;
    int64_t res = NEXT_STATE.REGS[n] - NEXT_STATE.REGS[m];
    if (d != 31)
    {
      NEXT_STATE.REGS[d] = res;
    }
    return;

}
void SUBS_Immediate()
{
    int64_t d = Decode_State.d;
    int64_t n = Decode_State.n;
    int64_t imm = Decode_State.imm;
    int64_t res = NEXT_STATE.REGS[n] - imm;
    if (res == 0)
    {
        NEXT_STATE.FLAG_Z = 1;
        NEXT_STATE.FLAG_N = 0;
    }
    else if (res < 0)
    {
        NEXT_STATE.FLAG_N = 1;
        NEXT_STATE.FLAG_Z = 0;
    }
    else
    {
        NEXT_STATE.FLAG_N = 0;
        NEXT_STATE.FLAG_Z = 0;
    }
    //for cmp(). discards result if d == 31
    if (d != 31)
    {
        NEXT_STATE.REGS[d] = res;
    }
    return;
}
void SUBS_Extended()
{
    int64_t d = Decode_State.d;
    int64_t n = Decode_State.n;
    int64_t m = Decode_State.m;
    int64_t res = NEXT_STATE.REGS[n] - NEXT_STATE.REGS[m];
    if (res == 0)
    {
        NEXT_STATE.FLAG_Z = 1;
        NEXT_STATE.FLAG_N = 0;
    }
    else if (res < 0)
    {
        NEXT_STATE.FLAG_N = 1;
        NEXT_STATE.FLAG_Z = 0;
    }
    else
    {
        NEXT_STATE.FLAG_N = 0;
        NEXT_STATE.FLAG_Z = 0;
    }
    //for cmp(). discards result if d == 31
    if (d != 31)
    {
        NEXT_STATE.REGS[d] = res;
    }
    return;
}
void MOVZ()
{
  if (Decode_State.d != 31)
  {
    NEXT_STATE.REGS[Decode_State.d] = Decode_State.imm;
  }
  return;
}
void MUL()
{
  if (Decode_State.d != 31)
  {
    NEXT_STATE.REGS[Decode_State.d] = NEXT_STATE.REGS[Decode_State.n] * NEXT_STATE.REGS[Decode_State.m];
  }
  return;
}
void HLT()
{
    RUN_BIT = FALSE;
    return;
}
void BR()
{
    NEXT_STATE.PC = NEXT_STATE.REGS[Decode_State.n];
    Decode_State.branching = 1;
    return;
}
void B()
{
    int64_t target = Decode_State.imm;
    Branch(target);
    return;
}
//cond is in d. imm is offset.
void B_Cond()
{
    int64_t cond = Decode_State.d;
    int64_t offset = Decode_State.imm >> 5;
    switch(cond)
    {
        //BEQ
        case(0):
            if (NEXT_STATE.FLAG_Z == 1)
            {Branch(offset);}
            break;
        //BNE
        case(1):
            if (NEXT_STATE.FLAG_Z == 0)
            {Branch(offset);}
            break;
        //BGE
        case(10):
            if ((NEXT_STATE.FLAG_Z == 1) || (NEXT_STATE.FLAG_N == 0))
            {Branch(offset);}
            break;
        //BLT
        case(11):
            if ((NEXT_STATE.FLAG_N == 1) && (NEXT_STATE.FLAG_Z == 0))
            {Branch(offset);}
            break;
        //BGT
        case(12):
            if ((NEXT_STATE.FLAG_N == 0) && (NEXT_STATE.FLAG_Z == 0))
            {Branch(offset);}
            break;
        //BLE
        case(13):
            if ((NEXT_STATE.FLAG_Z == 1) || (NEXT_STATE.FLAG_N == 1))
            {Branch(offset);}
            break;
    }
    //Branch Helper Function already sets .branching to 1
    return;
}


void execute()
{
    //I assume we will initialize the decode struct prior to execute()
    //using dummy variable for decode struct for now...
    switch(Decode_State.op)
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
        // Loads and Stores
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
    // keep as last line, moving program counter if we are not branching
    if (Decode_State.branching == 0)
    {
        NEXT_STATE.PC = CURRENT_STATE.PC + 4;
    }
    else {
        Decode_State.branching = 0;
    }
}

void process_instruction()
{
    /* execute one instruction here. You should use CURRENT_STATE and modify
     * values in NEXT_STATE. You can call mem_read_32() and mem_write_32() to
     * access memory. */
    fetch();
    decode();
    execute();

}
