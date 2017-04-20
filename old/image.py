#!/bin/evn python3

import array
import sys


def ps_byte(b):
	if b < 0x40:
		return " "
	elif b < 0x80:
		return "."
	elif b < 0xc0:
		return "o"
	else:
		return "8"

def conv(part, skip=0x6):
	res = bytes.fromhex(part)[skip:]
	res_len = len(res)
	pad_size = res[res_len - 1] + 1
	print(res_len - 0x20 - pad_size)

	return res[0: res_len - 0x20 - pad_size ]


# stream = conv(prnt_set1a) + conv(prnt_set1b) + conv(prnt_set1c)
# stream = conv(prnt_set2a) + conv(prnt_set2b) + conv(prnt_set2c)

# stream = conv(prnt_set3a) + conv(prnt_set3b) + conv(prnt_set3c)

p = prints[int(sys.argv[2])]
stream = conv(p[0], 0x12) + conv(p[1], 0x6) + conv(p[2], 0x6)

def frmt(bt):
	return "".join(map(ps_byte, bt))

# ar = array.array('B', stream)

l = len(stream)

p = int(sys.argv[1])
print("Part by ", p)

from PIL import Image

# for i in range(0, int(l / p)):
# 	print(frmt(stream[i * p: (i + 1) * p]))

# img = Image.frombytes('L', (144, 144), stream).save("test.png")

print("Part by ", sys.argv[3])
f = open(sys.argv[3], "rb")
Image.frombytes('L', (144, 144), f.read()).save("test.png")
