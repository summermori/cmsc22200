.text
mov X1, 1000
mov X2, 0

foo:
mov X3, 1
mov X3, 1
mov X3, 1
add X2, X2, 1
cmp X2, X1
bgt bar
blt foo

bar:
mov X1, 4
mov X2, 0


HLT 0