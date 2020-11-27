cmp x11, x11

bge foo

add x2, x0, 10

bar:

HLT 0

foo:

cmp x11, x11

bge bar

add x3, x0, 10

HLT 0
