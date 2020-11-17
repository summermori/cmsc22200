.text
mov X1, 1000
mov X4, 1000
mov X2, 0


foo:
mov X3, 1
mov X3, 1
mov X3, 1
sub X4, X4, 1
cbz X4, poo
cbnz X4, bar

bar:
add X2, X2, 1
cmp X1, X2
bgt foo


mov X1, 4
mov X2, 0

poo:
HLT 0
