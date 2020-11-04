.text
adds X8, X1, 0x1
adds X10, X9, X8
adds X11, X10, 0xff
ldur X10, [X11,0x4]
adds X12, X10, 1
adds X13, X11, 4
ldur X10, [X11,0x4]
stur X12, [X10, 0x4]
HLT 0
