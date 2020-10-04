#include <stdio.h>
#include "shell.h"

void fetch()
{
    uint32_t word = mem_read_32(CURRENT_STATE.PC);
    printf("%x\n",word);
    NEXT_STATE.PC = word;
}

void decode()
{

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
