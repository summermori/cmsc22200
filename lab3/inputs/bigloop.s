.text
mov X1, 1000
mov X2, 0
cmp X1, X2
beq hop1
blt hop2
bne hop3
bgt hop4

hop1:
mov X1, 1000
mov X2, 0
b hop4

hop2:
b hop5

hop3:
b foo

hop4:
add X2, X2, 1
cmp X1, X2
beq hop2
bne hop4

foo:
mov X3, 1
mov X3, 1
mov X3, 1
add X2, X2, 3
cmp X1, X2
bgt foo
blt hop1
b hop2

fee:
b hop7

hop5:
mov X1, 4
mov X2, 0
cmp X1, X2
beq hop1
blt hop2
cbnz X2,fee
cbz X1, fee
beq fee
bne fee

hop6:
b fuu

hop7:
b hop6

fuu:
HLT 0
