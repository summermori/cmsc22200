.text
mov X1, 1000
mov X2, 0

foo:
mov X3, 1
mov X3, 1
mov X3, 1
b baz

fu:
add X2, X2, 1
sub X5, X1, X2
cbz X5, bar
cbnz X5, foo

baz:
add X6, X6, 1
b fu

bar:
mov X1, 4
mov X2, 0


HLT 0
