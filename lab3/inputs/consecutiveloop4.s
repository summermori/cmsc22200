.text
mov X1, 1000
mov X2, 0

foo:
mov X3, 1
mov X3, 1
mov X3, 1
add X2, X2, 1
b baz

fu:
cmp X2, X1
beq bar
bne foo

baz:
add X6, X6, 1
b fu

bar:
mov X1, 4
mov X2, 0


HLT 0
