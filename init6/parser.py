#!/usr/bin/env python

from init6 import Init6
import sys
import json
import pprint
import hexdump
import io
import kaitaistruct

pp = pprint.PrettyPrinter(indent=2)
init6 = None

if sys.argv[1] == 'bin':
	init6 = Init6.from_file(sys.argv[2])
else:
	io = io.BytesIO(hexdump.restore(open(sys.argv[2], 'r').read()))
	init6 = Init6(kaitaistruct.KaitaiStream(io))

def str_intercept(data):
	if isinstance(data, bytes):
		return hexdump.hexdump(data, result='return')
	return str(data)

def str_fileds(self, fields):
	res = ''
	for field in fields:
		res += field + ':\n'
		res += '  ' + '\n  ' \
			.join(str_intercept(getattr(self, field)).split('\n')) + '\n'
	return res

def str_Init6(self):
	fields = ['data2', 'data3', 'data7']
	return str_fileds(self, fields)

Init6.__str__ = str_Init6
Init6.Data2.__str__ = lambda x: str_fileds(x, ['data1', 'encrypted', 'data2'])
Init6.Data3.__str__ = lambda x: str_fileds(x, ['header', 'data1', 'pad1', 'data2', 'pad2', 'data3'])
Init6.Data7.__str__ = lambda x: str_fileds(x, ['part1', 'signature'])

print(init6)
