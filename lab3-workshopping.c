//* memory light, computation expensive
typedef struct BTB_Entry {
	uint64_t addr_tag;
	unsigned char valid_bit;
	unsigned char	cond_bit;
	uint64_t target;

} btb_entry_t;

typedef struct bp
{
    unsigned char gshare;
    unsigned char pht[64];
    btb_entry_t btb_table[1024];

} bp_t;

//left-shift the binary number, set least-significant bit to new bit x, then take the 8 least-significant bits as the new gshare
//ex: gshare is 11010010
	//the last branch just hit, so x is 1
	//we shift gshare 1 to the left (shift = gshare << 1) {shift is 11010010}
	//we add/or x to shift (result = shift {+ ||} x) {result is 11010011}
	//we then set gshare to result (gshare = result) {gshare is 11010011}

void gshare_set (unsigned char x) {
	unsigned char shift = gshare << 1;
	BP.gshare = (shift | x);
}

//given a byte composed of bits [2:9] of the PC, XOR it with gshare, index that result into the pattern history table, and return the (first two bits/result) at that address
//ex: gshare is 11111111, pc_tag is 00000000
	//we XOR gshare with pc_tag (index = gshare ^ pc_tag) {index is 11111111}
	//we then need to index index in our pht struct. Each char inside holds 4 "entries", and we have 64 chars. To determine the correct char, we:
		//find the correct array index, which should be index/4 (arr_index = index/4) {arr_index = 63}
		//extract the relevant subarray using the array index (sub_arr = pht[arr_index])
		//find the correct char offset, which should be index%4 (char_offset = index%4) {char_offset = 3}
		//plug the char and the char offset into our magical helper function get_counter, which returns the two bits needed (two_bits = get_counter(sub_arr, char_offset))
	//we finally return the two bits (return two_bits)

unsigned char pht_check (unsigned char pc_tag) {
	unsigned char index = (BT.gshare ^ pc_tag);
	int arr_index = (index / 4);
	unsigned char sub_pht = BT.pht[arr_index];
	int char_offset = (index % 4);
	unsigned char two_bits = get_counter(sub_pht, char_offset);
	return two_bits;
}

//given an unsigned char sub_pht and an int char_offset, return the (char_offset * 2)th and ((char_offset * 2) + 1)th bytes
//ex: sub_pht is 10110001, char_offset is 3
	//we then go into a switch statement based on char_offset
		//if offset is 0, we use bitmask 11000000, and shift_val 6
		//if offset is 1, we use bitmask 00110000, and shift_val 4
		//if offset is 2, we use bitmask 00001100, and shift_val 2
		//if offset is 3, we use bitmask 00000011, and shift_val 0
		//if offset is 4, we go on strike
	//we then return sub_pht ANDed with bitmask and subsequentially rightshifted by shift_val

unsigned char get_counter(unsigned char sub_pht, int char_offset) {
	unsigned char bitmask = 0xC0 >> (char_offset * 2);
	int shift_val = 6 - (char_offset * 2);
	return ((sub_pht & bitmask) >> shift_val);
}

//given that get_counter DID NOT return 0 w/a miss and it DID NOT return 4 with a hit, repeat similar operations
//super important: if your main code calls pht_check, receives back 0 and gets a miss, OR it receives back 3 and gets a hit, YOU DO NOT CALL THIS FUNCTION
//this function just repeats the steps of pht_check up until the 'get_counter()' call, which is replaced by:
	//define unsigned char op_input, which is meant to be 0x1 left-shifted by the shift val (so if sub_pht is 10010011, and we're concerned with bits '01', op_input is 00010000)
	//we then determine what operation we're using: if inc is 1, we add, otherwise we subtract (if inc is 1, we add 10010011 and 00010000 to get 10100011, which is the correct new value)

void change_counter (unsigned char pc_tag, int inc) {
	unsigned char index = (BT.gshare ^ pc_tag);
	int arr_index = (index / 4);
	unsigned char sub_pht = BT.pht[arr_index];
	int char_offset = (index % 4);
	int shift_val = 6 - (char_offset * 2);
	unsigned char op_input = (0x1 << shift_val);
	if (inc == 1) {
		BT.pht[arr_index] = (sub_pht + op_input);
	}
	else {
		BT.pht[arr_index] = (sub_pht - op_input);
	}
}

btb_entry_t get_btb_entry(unsigned char pc_index) {
	return BP.btb_table[pc_index];
}

void update_btb_entry(unsigned char pc_index, uint64_t fetch_pc, unsigned char valid_bit, unsigned char cond_bit, uint64_t target) {
	BP_t entry = BP.btb_table[pc_index];
	entry.addr_tag = fetch_pc;
	entry.valid_bit = valid_bit;
	entry.cond_bit = cond_bit;
	entry.target =  target;
	BP.btb_table[pc_index] = entry;
}

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
	(inc > 0) ? valid_bit = 1 : valid_bit = 0;
	gshare_set(valid_bit);
	// 3 update the btb
	unsigned char btb_tag = (0x000007fe & fetch_pc) >> 1;
	update_btb_entry(btb_tag, fetch_pc, valid_bit, cond_bit, target);
}

// /* memory expensive, computation light
// typedef struct BTB_Entry {
// 	uint64_t addr_tag;
// 	int valid_bit;
// 	int cond_bit;
// 	uint64_t target;
//
// } btb_entry_t;
//
// typedef struct bp
// {
//     int gshare;
//     int_32_t pht[256];
//     btb_entry_t btb_table[1024];
//
// } bp_t;
