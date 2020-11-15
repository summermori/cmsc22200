.text
add X2, X4, 17
cbnz X2, bar
add X2, X0, 1

bar:
add X10, X2, X8
add X2, X4, 17
HLT 0
