.text
mov X3, 0x1
mov X4, 0x2
b foo
add X13, X0, 10

foo:
add X14, X9, 11
b bar

bar:
add X15, X2, X8

HLT 0
