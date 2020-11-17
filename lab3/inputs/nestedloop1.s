WIP

.text
mov X1, 1000
mov X2, 0

foo:
mov X3, 1
mov X3, 1
mov X3, 1
add X2, X2, 1
cmp X2, X1
bne foo

bar:

baz:
mov X1, 4
mov X2, 0


HLT 0
