/*
 * ARM pipeline timing simulator
 *
 * CMSC 22200, Fall 2016
 * Gushu Li and Reza Jokar
 */

#include "bp.h"
#include "pipe.h"
#include "shell.h"
#include <stdlib.h>
#include <stdio.h>


// helper functions
void gshare_set (unsigned char x) {
	unsigned char shift = BP.gshare << 1;
	BP.gshare = (shift | x);
}

unsigned char get_counter(unsigned char sub_pht, int char_offset) {
	unsigned char bitmask = 0xC0 >> (char_offset * 2);
	int shift_val = 6 - (char_offset * 2);
	return ((sub_pht & bitmask) >> shift_val);
}

unsigned char pht_check (unsigned char pc_tag) {
	unsigned char index = (BP.gshare ^ pc_tag);
	int arr_index = (index / 4);
	unsigned char sub_pht = BP.pht[arr_index];
	int char_offset = (index % 4);
	unsigned char two_bits = get_counter(sub_pht, char_offset);
        //printf("checking pht at index %x with arr_index %x and offset %x. result: %x\n", index, arr_index, char_offset, two_bits);
	return two_bits;
}

void change_counter (unsigned char pc_tag, int inc) {
	unsigned char index = (BP.gshare ^ pc_tag);
	int arr_index = (index / 4);
	unsigned char sub_pht = BP.pht[arr_index];
	int char_offset = (index % 4);
	int shift_val = 6 - (char_offset * 2);
	unsigned char op_input = (0x1 << shift_val);
	//printf("setting pht at index %x with arr_index %x and offset %x. op_in: %x\n", index, arr_index, char_offset, op_input);
	if (inc == 1) {
		BP.pht[arr_index] = (sub_pht + op_input);
	}
	else {
		BP.pht[arr_index] = (sub_pht - op_input);
	}
	//printf("PHT index: %d\n", arr_index);
	//printf("PHT entry: %d\n", BP.pht[arr_index]);
}

btb_entry_t get_btb_entry(unsigned char pc_index) {
	return BP.btb_table[pc_index];
}

void update_btb_entry(unsigned char pc_index, uint64_t fetch_pc, unsigned char valid_bit, unsigned char cond_bit, uint64_t target) {
	btb_entry_t entry = BP.btb_table[pc_index];
	entry.addr_tag = fetch_pc;
	entry.valid_bit = valid_bit;
	entry.cond_bit = cond_bit;
	entry.target =  target;
	BP.btb_table[pc_index] = entry;
}


// important functions
// we will have to make CURRENT_STATE a pointer to avoid undeclared variable issues
void bp_predict(uint64_t fetch_pc) {
	unsigned char gshare_tag = (0x000001fe & fetch_pc) >> 1;
	unsigned char btb_tag = (0x000007fe & fetch_pc) >> 1;
	unsigned char counter = pht_check(gshare_tag);
	btb_entry_t indexed_entry = get_btb_entry(btb_tag);
        printf("gshare: %x, gshare_tag: %x, predict counter: %x, btb_valid: %x, btb_cond: %x, btb_addr: %lx, fetch_pc: %lx\n", BP.gshare, gshare_tag, counter, indexed_entry.valid_bit, indexed_entry.cond_bit, indexed_entry.addr_tag, fetch_pc);
	//no hit BTB miss
	if ((indexed_entry.addr_tag != fetch_pc) || (indexed_entry.valid_bit == 0)) {
		// Control.prediction_taken = 0;
		// Control.pc_before_prediction = CURRENT_STATE.PC;
		printf("prediction: NOT TAKEN, fetch_pc != addr_tag OR valid_bit == 0\n");
		CURRENT_STATE.PC = fetch_pc + 4;
	}
	//hit 
	else if ((indexed_entry.cond_bit == 0) || (counter > 1)) {
                printf("prediction: TAKEN, cond_bit == 0 OR counter > 1\n");
		Control.prediction_taken = 1;
		Control.pc_before_prediction = CURRENT_STATE.PC;
		CURRENT_STATE.PC = indexed_entry.target;
		Control.taken_target = indexed_entry.target;
	}
	else {
		printf("prediction: NOT TAKEN, cond_bit == 0 AND counter <= 1\n");
		Control.prediction_taken = 0;
		Control.pc_before_prediction = CURRENT_STATE.PC;
		CURRENT_STATE.PC = fetch_pc + 4;
	}
}

// we still need a way to figure out if we should increment or decrement
// for now, int inc will indicate that
void bp_update(uint64_t fetch_pc, unsigned char cond_bit, uint64_t target, int inc) {
	if (cond_bit == 1)
	{
		// 1 update pht
		unsigned char gshare_tag = (0x000001fe & fetch_pc) >> 1;
		printf("bp_update: fetch_pc %lx, gshare_tag %x, inc %x\n", fetch_pc, gshare_tag, inc);
		unsigned char counter = pht_check(gshare_tag);
		if (!(((inc == 1) && (counter == 3)) || ((inc == -1) && (counter == 0)))) {
			//printf("updating gshare counter for tag %x\n", gshare_tag);
			change_counter(gshare_tag, inc);
		}
		// 2 update the ghr
		unsigned char valid_bit;
		(inc > 0) ? (valid_bit = 1) : (valid_bit = 0);
		//printf("gshare valid bit: %d\n", valid_bit);
		gshare_set(valid_bit);
		// 3 update the btb
		unsigned char btb_tag = (0x000007fe & fetch_pc) >> 1;
		update_btb_entry(btb_tag, fetch_pc, 1, cond_bit, target);
		//printf("btb_entry index: %d\n", btb_tag);
		//printf("updated btb_entry pc: %lx\n", BP.btb_table[btb_tag].addr_tag);
		//printf("updated btb_entry dest: %lx\n", BP.btb_table[btb_tag].target);
		//printf("updated btb_entry valid bit: %d\n", BP.btb_table[btb_tag].valid_bit);
		//printf("updated btb_entry cond bit: %d\n", BP.btb_table[btb_tag].cond_bit);
	}
	else if (cond_bit == 0)
	{
		// 3 update the btb
		unsigned char valid_bit;
		(inc > 0) ? (valid_bit = 1) : (valid_bit = 0);
		unsigned char btb_tag = (0x000007fe & fetch_pc) >> 1;
		update_btb_entry(btb_tag, fetch_pc, 1, cond_bit, target);
	}
	return;
}
