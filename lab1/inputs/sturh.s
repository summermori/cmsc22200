.text
mov X1, 0x1000
lsl X1, X1, 16
mov X10, 0x1234
stur X10, [X1, 0x0]
sturh W10, [X1, 0x6]
ldur X13, [X1, 0x0]
ldur X14, [X1, 0x4]
ldurh W15, [X1, 0x6]
HLT 0
