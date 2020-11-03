.text

mov X9, #0x82
sturb W9, [X10, #0x7]
mov X9, #0x7F
sturb W9, [X10, #0xF]

ldursb W22, [X10, #0x7]
ldursb W23, [X10, #0xF]

ldursh W24, [X10, #0x6]
ldursh W25, [X10, #0xE]

// No sign extension happens for 32 bit load
ldur W26, [X10, #0x4]
ldur W27, [X10, #0xC]

HLT 0
