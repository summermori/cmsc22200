.text
add X3, X4, 17
cbz X2, bar
add X2, X0, 1

foo:
add X8, X9, 13
b bar

bar:
add X10, X2, X8

add X2, X4, 17
cbnz X3, bar2
add X8, X9, 19
cbz X3, bar2
add X2, X0, 11

foo2:
add X8, X9, 1
b bar2

bar2:
add X10, X2, X8


lsl X0, X11, 3
lsr X0, X11, 2

cbz X12, target
add X1, X2, X3
target:


cbnz X12, target2
add X1, X2, X3
target2:


mov X9, #3
mov X10, #8
HLT 0 
