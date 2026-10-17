"""Derive unique wildcarded byte patterns for engine functions.

Given an amtrucks.exe and known-good RVAs (verified in game), grow a pattern
instruction by instruction from the function start until it matches exactly
once in .text. Build-specific bytes (rel32 targets, RIP-relative
displacements) are wildcarded; struct offsets and opcodes are kept so a
semantic change in a later build makes the pattern fail instead of matching
the wrong code.

    python3 -I make_patterns.py <amtrucks.exe> <insn_mask binary>
"""
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from pe import PE  # noqa: E402

# Verified on ATS 1.61.3.18 (exe SHA-256 C60E8EE0...) by the original mod's
# in-game run on 2026-10-17.
KNOWN = {
    'model_load': 0x15DCB20,
    'model_transfer': 0x33DEF0,
    'model_parameter_init': 0x41E020,
    'set_transform': 0x13CF940,
    'vehicle_render_dispatch': 0x799F30,
}
MIN_BYTES = 40
MAX_INSNS = 40


def instructions(tool, exe, offset, count):
    out = subprocess.run([tool, exe, f'{offset:x}', str(count)], capture_output=True,
                         text=True, check=True).stdout.split('\n')
    result = []
    for line in out:
        if not line or line == 'ERR':
            break
        length, mask = line.split()
        result.append((int(length), mask))
    return result


def to_regex(data, mask):
    return b''.join(b'.' if m == '?' else re.escape(bytes([b])) for b, m in zip(data, mask))


def main():
    exe, tool = sys.argv[1], sys.argv[2]
    pe = PE(exe)
    text_va, text = pe.text()
    for name, rva in KNOWN.items():
        off = pe.rva_to_off(rva)
        insns = instructions(tool, exe, off, MAX_INSNS)
        data = b''
        mask = ''
        found = None
        for length, m in insns:
            data += pe.data[off + len(data):off + len(data) + length]
            mask += m
            if len(data) < MIN_BYTES:
                continue
            regex = re.compile(to_regex(data, mask), re.DOTALL)
            hits = [text_va + h.start() for h in regex.finditer(text)]
            if len(hits) == 1:
                assert hits[0] == rva, (name, hex(hits[0]))
                found = (data, mask)
                break
        if not found:
            print(f'{name}: NOT UNIQUE within {MAX_INSNS} instructions', file=sys.stderr)
            continue
        data, mask = found
        pattern = ' '.join('??' if m == '?' else f'{b:02X}' for b, m in zip(data, mask))
        print(f'{name} {len(data)} {pattern}')


if __name__ == '__main__':
    main()
