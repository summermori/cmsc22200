.text
mov X1, 0x1000
lsl X1, X1, 16
mov X10, 10
sturb W10, [X1, 0x0]
mov X12, 2
sturb W12, [X1, 0x10]
sturb W12, [X1, 0x10]
ldurb W13, [X1, 0x0]
ldurb W14, [X1, 0x10]
HLT 0
