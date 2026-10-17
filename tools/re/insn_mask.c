// Prints, for code at a file offset, each instruction's length and a mask of
// bytes that are build-specific (rel32 branch targets and RIP-relative
// displacements). Used by make_patterns.py to build wildcarded signatures.
//   insn_mask <file> <offset-hex> <count>
// Output per instruction: "<len> <mask>" where mask has one char per byte:
// 'x' = keep, '?' = wildcard.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../third_party/minhook/src/hde/hde64.h"

int main(int argc, char **argv)
{
    if (argc < 4) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 1;
    long off = strtol(argv[2], NULL, 16);
    int count = atoi(argv[3]);
    unsigned char buf[4096];
    fseek(f, off, SEEK_SET);
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    size_t pos = 0;
    for (int i = 0; i < count && pos + 16 < n; ++i)
    {
        hde64s hs;
        unsigned len = hde64_disasm(buf + pos, &hs);
        if (hs.flags & F_ERROR || len == 0) { printf("ERR\n"); return 3; }
        char mask[32];
        memset(mask, 'x', len);
        mask[len] = 0;
        unsigned imm = 0;
        if (hs.flags & F_IMM8) imm += 1;
        if (hs.flags & F_IMM16) imm += 2;
        if (hs.flags & F_IMM32) imm += 4;
        if (hs.flags & F_IMM64) imm += 8;
        // Relative branch/call: the immediate is a build-specific target.
        if ((hs.flags & F_RELATIVE) && imm >= 4)
            memset(mask + len - 4, '?', 4);
        // RIP-relative memory operand: disp32 sits before any immediate.
        if ((hs.flags & F_MODRM) && hs.modrm_mod == 0 && hs.modrm_rm == 5 && (hs.flags & F_DISP32))
            memset(mask + len - imm - 4, '?', 4);
        printf("%u %s\n", len, mask);
        // Stop after an unconditional return/jump at function level.
        pos += len;
    }
    return 0;
}
