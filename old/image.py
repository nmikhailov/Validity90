#!/bin/evn python3

import array
import sys

from PIL import Image


f = open(sys.argv[1], "rb")
data = bytes.fromhex(f.read().decode("utf-8"))
Image.frombytes('L', (120, 112), data).save("test.png")
