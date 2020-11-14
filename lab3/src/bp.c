/*
 * ARM pipeline timing simulator
 *
 * CMSC 22200, Fall 2016
 * Gushu Li and Reza Jokar
 */

#include "bp.h"
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
	return two_bits;
}

void change_counter (unsigned char pc_tag, int inc) {
	unsigned char index = (BP.gshare ^ pc_tag);
	int arr_index = (index / 4);
	unsigned char sub_pht = BP.pht[arr_index];
	int char_offset = (index % 4);
	int shift_val = 6 - (char_offset * 2);
	unsigned char op_input = (0x1 << shift_val);
	if (inc == 1) {
		BP.pht[arr_index] = (sub_pht + op_input);
	}
	else {
		BP.pht[arr_index] = (sub_pht - op_input);
	}
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
	if ((indexed_entry.addr_tag != fetch_pc) || (indexed_entry.valid_bit == 0)) {
		CURRENT_STATE.PC = fetch_pc + 4;
	}
	else if ((indexed_entry.cond_bit == 0) || (counter > 1)) {
		CURRENT_STATE.PC = indexed_entry.target;
	}
	else {
		CURRENT_STATE.PC = fetch_pc + 4;
	}
}

// we still need a way to figure out if we should increment or decrement
// for now, int inc will indicate that
void bp_update(uint64_t fetch_pc, unsigned char cond_bit, uint64_t target, int inc) {
	// 1 update pht
	unsigned char gshare_tag = (0x000001fe & fetch_pc) >> 1;
	unsigned char counter = pht_check(gshare_tag);
	if (!(((inc == 1) && (counter == 3)) || ((inc == -1) && (counter == 0)))) {
		change_counter(gshare_tag, inc);
	}
	// 2 update the ghr
	unsigned char valid_bit;
	(inc > 0) ? (valid_bit = 1) : (valid_bit = 0);
	gshare_set(valid_bit);
	// 3 update the btb
	unsigned char btb_tag = (0x000007fe & fetch_pc) >> 1;
	update_btb_entry(btb_tag, fetch_pc, valid_bit, cond_bit, target);
}