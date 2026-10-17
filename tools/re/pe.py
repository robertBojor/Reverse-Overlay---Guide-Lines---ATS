"""Minimal PE helpers for locating code in amtrucks.exe (stdlib only)."""
import struct


class PE:
    def __init__(self, path):
        self.data = open(path, 'rb').read()
        d = self.data
        e_lfanew = struct.unpack_from('<I', d, 0x3C)[0]
        assert d[e_lfanew:e_lfanew + 4] == b'PE\0\0'
        nsec = struct.unpack_from('<H', d, e_lfanew + 6)[0]
        opt_size = struct.unpack_from('<H', d, e_lfanew + 20)[0]
        opt = e_lfanew + 24
        self.image_base = struct.unpack_from('<Q', d, opt + 24)[0]
        self.sections = []
        sec = opt + opt_size
        for i in range(nsec):
            name = d[sec:sec + 8].rstrip(b'\0').decode()
            vsize, va, rsize, raw = struct.unpack_from('<IIII', d, sec + 8)
            self.sections.append((name, va, vsize, raw, rsize))
            sec += 40

    def section(self, name):
        for s in self.sections:
            if s[0] == name:
                return s
        raise KeyError(name)

    def rva_to_off(self, rva):
        for name, va, vsize, raw, rsize in self.sections:
            if va <= rva < va + max(vsize, rsize):
                return raw + (rva - va)
        raise ValueError(hex(rva))

    def off_to_rva(self, off):
        for name, va, vsize, raw, rsize in self.sections:
            if raw <= off < raw + rsize:
                return va + (off - raw)
        raise ValueError(hex(off))

    def text(self):
        name, va, vsize, raw, rsize = self.section('.text')
        return va, self.data[raw:raw + min(vsize, rsize)]

    def bytes_at(self, rva, n):
        off = self.rva_to_off(rva)
        return self.data[off:off + n]
