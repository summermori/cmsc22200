//  Steps for the read function
//  1. Get Bitmasks
//  2. Index according to associativy (separate helper function)
//    2a. Checking is done in indexing, check valid bits first
//  3. 
//    3a. On hit, get data from block, and update order
//    3b. On miss, buffer for 50 cycles and replace data (helper for evictions)
//      3b1.  Use PC to track order
//      3b2.  Look through every entry to see if they are empty, but also keep track of the minium
  
 // Structs
 struct instruction_block {
   uint_32 tag;
   unsigned char valid;
   unsigned char dirty;
   uint_32 order; // check what the PC global variable type is
   uint_32 data[4];
} inst_block;

struct data_block {
   uint_32 tag;
   unsigned char valid;
   unsigned char dirty;
   uint_32 order;
   uint_32 data[8];
} data_block;
