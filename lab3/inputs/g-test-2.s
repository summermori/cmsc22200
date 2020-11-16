.text
add X5, X2, X1
ldur X3, [X5, 4]
ldur X2, [X2, 0]
orr X3, X5, X3
stur X3, [X5, 0]
HLT 0
