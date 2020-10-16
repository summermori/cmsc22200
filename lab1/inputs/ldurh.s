.text
mov X1, 0x1000
lsl X1, X1, 16
mov X10, 10
sturh W10, [X1, 0x0]
mov X12, 2
sturh W12, [X1, 0x10]
sturh W12, [X1, 0x10]
ldurh W13, [X1, 0x0]
ldurh W14, [X1, 0x10]
HLT 0
