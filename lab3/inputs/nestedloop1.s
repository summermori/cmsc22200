.text
mov X1, 100
mov X2, 0
mov X10, 10
mov X11, 0

foo:
mov X3, 1
mov X3, 1
mov X3, 1
b bar

fu:
mov X11, 0
add X2, X2, 1
cmp X2, X1
beq baz
bne foo

bar:
add X11, X11, 1
cmp X11, X10
beq fu
bne bar

baz:
mov X1, 4
mov X2, 0


HLT 0
