.text
mov X1, 1000
mov X2, 0
b foo

fee:
HLT 0

foo:
mov X3, 1
mov X3, 1
mov X3, 1
add X2, X2, 1
cmp X1, X2
bgt foo
b fee

mov X2, 0

HLT 0
