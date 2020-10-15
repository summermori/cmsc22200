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
    printf("%x\n",word);
    NEXT_STATE.PC = word;
}
void decode()
{
  int word = NEXT_STATE.PC;
  int temp = word & 0x1E000000;
  // Data Processing - Immediate
  if ((temp == 0x10000000) || (temp == 0x12000000)) {
    printf("Data Processing - Immediate, ");
    int temp2 = word & 0x03800000;
    // Add/subtract (immediate)
    if ((temp2 == 0x01000000) || (temp2 == 0x01800000)) {
      Decode_State.op = (word & 0xff000000);
      Decode_State.imm = (word & 0x003ffc00) >> 10;
      Decode_State.n = (word & 0x000003e0) >> 5;
      Decode_State.d = (word & 0x0000001f);
      printf("Add/subtraction (immediate), ");
    }
    // Move wide (immediate)
    else if (temp2 == 0x02800000) {
      Decode_State.op = (word & 0xff800000);
      Decode_State.imm = (word & 0x001fffe0) >> 5;
      Decode_State.d = (word & 0x0000001f);
      printf("Move wide (immediate), ");
    }
    // Bitfield
    else if (temp2 == 0x03000000) {
      // this should be reviewed, since i am unsure what you guys need - victor
      Decode_State.op = (word & 0xff800000);
      Decode_State.n = (word & 0x000003e0) >> 5;
      Decode_State.d = (word & 0x0000001f);
      Decode_State.imm = (word & 0x0000fc00) >> 10;
      Decode_State.m = (word & 0x003f0000) >> 16; // using m as another imm
      printf("Bitfield, ");
    }
    else {
      printf("Failure to match subtype");
    }
  }

  // Branches and Exceptions
  else if ((temp == 0x14000000) || (temp == 0x16000000)) {
    printf("Branches and Exceptions, ");
    // Conditional branch
    if ((word & 0xfe000000) == 0x54000000) {
      Decode_State.op = (word & 0xff000000);
      Decode_State.d = (word & 0x0000000f);
      Decode_State.imm = (word & 0x00ffffe0) >> 5;
      // sam, what should the inverse be? i didn't see it in the specs - victor
      printf("Conditional branch, ");
    }
    // Exception
    else if ((word & 0xff000000) == 0xd4000000) {
      Decode_State.op = (word & 0xffe00000);
      Decode_State.imm = (word & 0x001fffe0) >> 5;
      printf("Exception, ");
    }
    // Unconditional branch (register)
    else if ((word & 0xfe000000) == 0xd6000000) {
      Decode_State.op = (word & 0xfffffc00);
      Decode_State.n = (word & 0x000003e0) >> 5;
      printf("Unconditional branch (register), ");
    }
    // Unconditional branch (immediate)
    else if ((word & 0x60000000) == 0) {
      Decode_State.op = (word & 0xfc000000);
      Decode_State.imm = (word & 0x03ffffff);
      printf("Unconditional branch (immediate), ");
    }
    // Compare and branch
    else if ((word & 0x7e000000) == 0x34000000) {
      Decode_State.op = (word & 0xff000000);
      Decode_State.branch_to = (word & 0x0000001f);
      printf("Compare and branch, ");
    }
    else {
      printf("Failure to match subtype");
    }
  }
  // Loads and Stores
  else if ((word & 0x0a000000) == 0x08000000) {
    Decode_State.op = (word & 0xffc00000);
    Decode_State.n = (word & 0x000003e0) >> 5;
    Decode_State.d = (word & 0x0000001f);
    Decode_State.imm = (word & 0x001ff000) >> 12;
    Decode_State.m = word;
    printf("Loads and Stores, Load/store (unscaled immediate), ");
  }
  // Data Processing - Register
  else if ((word & 0x0e000000) == 0x0a000000) {
    printf("Data Processing - Register, ");
    // Logical (shifted register)
    if ((word & 0x1f000000) == 0x0a000000) {
      Decode_State.op = (word & 0xff000000);
      Decode_State.m = (word & 0x001f0000) >> 16;
      Decode_State.n = (word & 0x000003e0) >> 5;
      Decode_State.d = (word & 0x0000001f);
      Decode_State.imm = (word & 0x0000fc00) >> 10;
      // Decode_State.shift = (word & 0x00c00000) >> 22; // i don't know how you guys would like me to store this - victor
      printf("Logical (shifted register), ");
    }
    // Add/subtract (extended register)
    else if ((word & 0x1f200000) == 0x0b000000) {
      Decode_State.op = (word & 0xffe00000);
      Decode_State.m = (word & 0x001f0000) >> 16;
      Decode_State.n = (word & 0x000003e0) >> 5;
      Decode_State.d = (word & 0x0000001f);
      printf("Add/subtract (extended register), ");
    }
    // Data processing (3 source)
    else if ((word & 0x1f000000) == 0x1b000000) {
      Decode_State.op = (word & 0xffe00000);
      Decode_State.m = (word & 0x001f0000) >> 16;
      Decode_State.n = (word & 0x000003e0) >> 5;
      Decode_State.d = (word & 0x0000001f);
      printf("Data processing (3 source), ");
    }
    else {
      printf("Failure to match subtype");
    }
  }
  else {
    printf("Failure to match subtype");
  }
}

void execute()
{
    // keep as last line, moving program counter
    NEXT_STATE.PC = CURRENT_STATE.PC + 4;
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
