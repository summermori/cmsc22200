.text
mov X1, 1000
mov X2, 0

foo:
mov X3, 1
mov X3, 1
mov X3, 1
sub X1, X1, 1
cbnz X1, foo

mov X1, 4
mov X2, 0


HLT 0
