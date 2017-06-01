# This is a generated file! Please edit source .ksy file and use kaitai-struct-compiler to rebuild

import array
import struct
import zlib
from enum import Enum
from pkg_resources import parse_version

from kaitaistruct import __version__ as ks_version, KaitaiStruct, KaitaiStream, BytesIO


if parse_version(ks_version) < parse_version('0.7'):
    raise Exception("Incompatible Kaitai Struct Python API: 0.7 or later is required, but you have %s" % (ks_version))

class Init6(KaitaiStruct):
    def __init__(self, _io, _parent=None, _root=None):
        self._io = _io
        self._parent = _parent
        self._root = _root if _root else self
        self.const1 = self._io.ensure_fixed_contents(struct.pack('11b', 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 1))
        self.hash1 = self._root.Hash(self._io, self, self._root)
        self.const2 = self._io.ensure_fixed_contents(struct.pack('6b', 29, 0, 4, 0, -95, 0))
        self.data2_hash = self._root.Hash(self._io, self, self._root)
        self._raw_data2 = self._io.read_bytes(161)
        io = KaitaiStream(BytesIO(self._raw_data2))
        self.data2 = self._root.Data2(io, self, self._root)
        self.const3 = self._io.ensure_fixed_contents(struct.pack('4b', 3, 0, -72, 0))
        self.data3_hash = self._root.Hash(self._io, self, self._root)
        self._raw_data3 = self._io.read_bytes(184)
        io = KaitaiStream(BytesIO(self._raw_data3))
        self.data3 = self._root.Data3(io, self, self._root)
        self.const4 = self._io.ensure_fixed_contents(struct.pack('4b', 5, 0, -92, 1))
        self.data4_hash = self._root.Hash(self._io, self, self._root)
        self.data4 = self._io.read_bytes(420)
        self.const5 = self._io.ensure_fixed_contents(struct.pack('4b', 1, 0, 0, 1))
        self.data5_hash = self._root.Hash(self._io, self, self._root)
        self.data5 = self._io.read_bytes(256)
        self.const6 = self._io.ensure_fixed_contents(struct.pack('4b', 2, 0, 0, 1))
        self.data6_hash = self._root.Hash(self._io, self, self._root)
        self.data6 = self._io.read_bytes(256)
        self.const7 = self._io.ensure_fixed_contents(struct.pack('4b', 6, 0, -112, 1))
        self.data7_hash = self._root.Hash(self._io, self, self._root)
        self._raw_data7 = self._io.read_bytes(400)
        io = KaitaiStream(BytesIO(self._raw_data7))
        self.data7 = self._root.Data7(io, self, self._root)

    class Hash(KaitaiStruct):
        def __init__(self, _io, _parent=None, _root=None):
            self._io = _io
            self._parent = _parent
            self._root = _root if _root else self
            self.hash = self._io.read_bytes(32)


    class Data2(KaitaiStruct):
        def __init__(self, _io, _parent=None, _root=None):
            self._io = _io
            self._parent = _parent
            self._root = _root if _root else self
            self.data1 = self._io.read_bytes(17)
            self.encrypted = self._io.read_bytes(112)
            self.data2 = self._io.read_bytes(32)


    class Data3(KaitaiStruct):
        def __init__(self, _io, _parent=None, _root=None):
            self._io = _io
            self._parent = _parent
            self._root = _root if _root else self
            self.header = self._io.read_bytes(8)
            self.data1 = self._io.read_bytes(32)
            self.pad1 = self._io.read_bytes(36)
            self.data2 = self._io.read_bytes(32)
            self.pad2 = self._io.read_bytes(44)
            self.data3 = self._io.read_bytes(32)


    class Data7(KaitaiStruct):
        def __init__(self, _io, _parent=None, _root=None):
            self._io = _io
            self._parent = _parent
            self._root = _root if _root else self
            self.part1 = self._io.read_bytes(144)
            self.signature = self._io.read_bytes(76)
            self.part2 = self._io.read_bytes_full()



