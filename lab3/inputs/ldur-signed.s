.text

mov X9, #0x80
sturb W9, [X10, #0x7]
mov X9, #0x7F
sturb W9, [X10, #0xF]

ldursb X22, [X10, #0x7]
ldursb X23, [X10, #0xF]

ldursh X24, [X10, #0x6]
ldursh X25, [X10, #0xE]

ldursw X26, [X10, #0x4]
ldursw X27, [X10, #0xC]

// No sign extension happens for 64 bit load
ldur X28, [X10, #0x0]
ldur X29, [X10, #0x8]

HLT 0
