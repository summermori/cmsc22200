.text
add X9, X8, 10
add X10, X8, 7
lsl X12, X10, 1
lsl X13, X9, 2
lsl X14, X9, 0

add X9, X8, 0x100
lsr X13, X9, 2
lsr X14, X9, 0
lsr X15, X9, 1
lsr X16, X9, 2
lsr X17, X9, 33
lsr X18, X9, 63

mov X10, #8
mov X11, #9
mul X12, X10, X11
mul X13, X10, X10
HLT 0
