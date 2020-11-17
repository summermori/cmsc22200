.text
mov X1, 1000
mov X2, 0

foo:
mov X3, 1
mov X3, 1
mov X3, 1
add X2, X2, 1
sub X5, X1, X2
cbz X5, bar
cbnz X5, foo

bar:
mov X1, 4
mov X2, 0


HLT 0
