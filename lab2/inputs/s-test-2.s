.text
mov X1, 0x1000
lsl X1, X1, 16
ldur X6, [X1, 0x0]
stur X6, [x1, 0x8]
HLT 0
