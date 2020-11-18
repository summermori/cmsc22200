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
	return BP.pht[index];
}

void change_counter (unsigned char pc_tag, int inc) {
	unsigned char index = (BP.gshare ^ pc_tag);
	unsigned char temp = BP.pht[index];
	BP.pht[index] = temp + inc;
	printf("PHT index: %d\n", index);
	printf("PHT counter: %d\n", BP.pht[index]);
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
	printf("fetch_pc in bp_predict: %lx\n", fetch_pc);
	// unsigned char gshare_tag = (0x000001fe & fetch_pc) >> 1;
	// unsigned char btb_tag = (0x000007fe & fetch_pc) >> 1;
	unsigned char gshare_tag = (0x000001fe & fetch_pc) >> 2;
	unsigned char btb_tag = (0x000007fe & fetch_pc) >> 2;
	unsigned char counter = pht_check(gshare_tag);
	btb_entry_t indexed_entry = get_btb_entry(btb_tag);
	//no hit BTB miss
	if ((indexed_entry.addr_tag != fetch_pc) || (indexed_entry.valid_bit == 0)) {
		struct Prediction temp;
		temp.prediction_taken = 0;
		temp.pc_before_prediction = CURRENT_STATE.PC;
		printf("loaded_pc in bp_predict BTB miss: %x\n", temp.pc_before_prediction);
		enqueue(&q, temp);
		if (q.head == NULL)
		{
			printf("predict head is null\n");
		}
		else
		{
			printf("predict pc_before_prediction in bp_predict head: %x\n", q.head->pred.pc_before_prediction);
		}
		if (q.tail == NULL)
		{
			printf("predict tail is null\n");
		}
		else
		{
			printf("predict pc_before_prediction in bp_predict tail: %x\n", q.tail->pred.pc_before_prediction);
		}
		// Control.prediction_taken = 0;
		// Control.pc_before_prediction = CURRENT_STATE.PC;
		CURRENT_STATE.PC = fetch_pc + 4;
	}
	//hit 
	else if ((indexed_entry.cond_bit == 0) || (counter > 1)) {
		printf("prediction hit!\n");
		struct Prediction temp;
		temp.prediction_taken = 1;
		temp.pc_before_prediction = CURRENT_STATE.PC;
		temp.taken_target = indexed_entry.target;
		enqueue(&q, temp);
		
		//no bubble on hit rn;
		CURRENT_STATE.PC = indexed_entry.target;
	}
	else {
		struct Prediction temp;
		temp.prediction_taken = 0;
		temp.pc_before_prediction = CURRENT_STATE.PC;
		printf("loaded_pc in bp_predict BTB hit but cond miss: %x\n", temp.pc_before_prediction);
		enqueue(&q, temp);
		CURRENT_STATE.PC = fetch_pc + 4;
	}
}

// we still need a way to figure out if we should increment or decrement
// for now, int inc will indicate that
void bp_update(uint64_t fetch_pc, unsigned char cond_bit, uint64_t target, int inc) {
	if (cond_bit == 1)
	{
		// 1 update pht
		// unsigned char gshare_tag = (0x000001fe & fetch_pc) >> 1;
		unsigned char gshare_tag = (0x000001fe & fetch_pc) >> 2;
		unsigned char counter = pht_check(gshare_tag);
		if (!(((inc == 1) && (counter == 3)) || ((inc == -1) && (counter == 0)))) {
			change_counter(gshare_tag, inc);
		}
		// 2 update the ghr
		unsigned char valid_bit;
		(inc > 0) ? (valid_bit = 1) : (valid_bit = 0);
		gshare_set(valid_bit);
		printf("gshare: %x\n", BP.gshare);
		// 3 update the btb
		// unsigned char btb_tag = (0x000007fe & fetch_pc) >> 1;
		unsigned char btb_tag = (0x000007fe & fetch_pc) >> 2;

		update_btb_entry(btb_tag, fetch_pc, 1, cond_bit, target);
		printf("btb_entry index: %d\n", btb_tag);
		printf("updated btb_entry pc: %lx\n", BP.btb_table[btb_tag].addr_tag);
		printf("updated btb_entry dest: %lx\n", BP.btb_table[btb_tag].target);
		printf("updated btb_entry valid bit: %d\n", BP.btb_table[btb_tag].valid_bit);
		printf("updated btb_entry cond bit: %d\n", BP.btb_table[btb_tag].cond_bit);
	}
	else if (cond_bit == 0)
	{
		// 3 update the btb
		unsigned char btb_tag = (0x000007fe & fetch_pc) >> 2;
		update_btb_entry(btb_tag, fetch_pc, 1, cond_bit, target);

		printf("btb_entry index: %d\n", btb_tag);
		printf("updated btb_entry pc: %lx\n", BP.btb_table[btb_tag].addr_tag);
		printf("updated btb_entry dest: %lx\n", BP.btb_table[btb_tag].target);
		printf("updated btb_entry valid bit: %d\n", BP.btb_table[btb_tag].valid_bit);
		printf("updated btb_entry cond bit: %d\n", BP.btb_table[btb_tag].cond_bit);
	}
	return;
}
