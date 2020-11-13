.text
mov X9, #3
mov X10, #8
sub X11, X0, #2

add X9, X8, 10
add X10, X8, 7
add X11, X8, 23
eor X12, X10, X11
eor X13, X10, X9

mov X1, 0x1000
lsl X1, X1, 16
mov X10, 10
stur X10, [X1, 0x0]
mov X12, 2
stur X12, [X1, 0x10]
ldur X13, [X1, 0x0]
ldur X14, [X1, 0x10]
HLT 0 
