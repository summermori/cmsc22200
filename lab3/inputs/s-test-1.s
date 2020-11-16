.text
mov X1, 0x1000
lsl X1, X1, 16
ldur X6, [X1, 0x0]
adds X8, X6, 0x1
HLT 0
