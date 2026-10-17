// Function fingerprints for build-to-build matching.
//   fnscan <file>        reads "rva file_offset size" lines on stdin
// For each function prints one line:
//   <rva> <size> <tok,tok,...> ; <call_rva,call_rva,...>
// A token encodes the instruction shape without operands: REX.W, 0F escape,
// opcode, ModRM.reg for group opcodes and register-vs-memory form. Call
// targets are the RVAs of direct E8 calls, in order.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../third_party/minhook/src/hde/hde64.h"

static int is_group_opcode(unsigned op)
{
    return (op >= 0x80 && op <= 0x83) || op == 0xC0 || op == 0xC1 || (op >= 0xD0 && op <= 0xD3) ||
           op == 0xF6 || op == 0xF7 || op == 0xFE || op == 0xFF;
}

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *image = malloc((size_t)file_size + 32);
    if (!image || fread(image, 1, (size_t)file_size, f) != (size_t)file_size) return 1;
    memset(image + file_size, 0xCC, 32);
    fclose(f);

    unsigned long rva, off, size;
    while (scanf("%lx %lx %lx", &rva, &off, &size) == 3)
    {
        if (off + size > (unsigned long)file_size) continue;
        printf("%lx %lx ", rva, size);
        unsigned long pos = 0;
        int first = 1;
        // Calls are collected and printed after the tokens.
        unsigned long calls[512];
        int ncalls = 0;
        while (pos < size)
        {
            hde64s hs;
            unsigned len = hde64_disasm(image + off + pos, &hs);
            if ((hs.flags & F_ERROR) || len == 0) break;
            unsigned op = hs.opcode == 0x0F ? hs.opcode2 : hs.opcode;
            unsigned tok = ((hs.rex_w & 1u) << 13) | ((hs.opcode == 0x0F) << 12) | (op << 4);
            if ((hs.flags & F_MODRM) && is_group_opcode(hs.opcode)) tok |= (hs.modrm_reg & 7u) << 1;
            if ((hs.flags & F_MODRM) && hs.modrm_mod == 3) tok |= 1u;
            printf(first ? "%x" : ",%x", tok);
            first = 0;
            if (hs.opcode == 0xE8 && len == 5 && ncalls < 512)
            {
                int rel;
                memcpy(&rel, image + off + pos + 1, 4);
                calls[ncalls++] = rva + pos + 5 + (long)rel;
            }
            pos += len;
        }
        printf(" ;");
        for (int i = 0; i < ncalls; ++i) printf(i ? ",%lx" : " %lx", calls[i]);
        printf("\n");
    }
    free(image);
    return 0;
}
