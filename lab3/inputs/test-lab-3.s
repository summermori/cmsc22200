.text
add X9, X9, 0xa
top:
cmp X11, X11
beq foo
add X2, X0, 10
add X5, X0, 10
add X6, X0, 10
add X7, X0, 10

bar:
cmp X10, X9
bne top
HLT 0

foo:
add X10, X10, 0x1
cmp X11, X11
beq bar
add X3, X0, 10
HLT 0
