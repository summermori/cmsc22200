.text
mov X1, 0x1000
lsl X1, X1, 16
mov X10, 0x1234
stur X10, [X1, 0x0]
sturb W10, [X1, 0x10]
ldur X13, [X1, 0x0]
ldurb W14, [X1, 0x10]


mov X1, 0x1000
lsl X1, X1, 16
mov X10, 0x1234
stur X10, [X1, 0x0]
sturh W10, [X1, 0x10]
ldur X13, [X1, 0x0]
ldurh W14, [X1, 0x10]
HLT 0
