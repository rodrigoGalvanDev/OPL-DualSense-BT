/*
 * Host test of the 2 port poll layout and split (modules/pademu/pademu_split.h).
 *   gcc -Wall -Wextra -o test_pademu_split tools/test_pademu_split.c && ./test_pademu_split
 */
#include <stdio.h>
#include <string.h>
#include "../modules/pademu/pademu_split.h"

static int failures, checks;

static void expect(const char *what, long got, long want)
{
    checks++;
    if (got != want) {
        failures++;
        printf("FAIL: %s: got %ld, want %ld\n", what, got, want);
    }
}

#define REG(port, len) ((unsigned)(port) | ((unsigned)(len) << 8) | ((unsigned)(len) << 18) | 0x40)

/* A model of the console's SIO2: walks the queue, every command is answered 0xA0 + port, and the walk stops at length 0 */
typedef struct
{
    unsigned regdata[16];
    unsigned in_size;
    unsigned char *in, *out;
} fake_td;

static int hw_commands;
static int hw_ports_seen[4];

static void fake_sio2(fake_td *td)
{
    unsigned pos = 0, i;

    for (i = 0; i < 16 && pademu_cmd_len(td->regdata[i]); i++) {
        unsigned len = pademu_cmd_len(td->regdata[i]), b;

        hw_commands++;
        hw_ports_seen[td->regdata[i] & 3]++;
        for (b = 0; b < len; b++)
            td->out[pos + b] = (unsigned char)(0xA0 + (td->regdata[i] & 3));
        pos += len;
    }
}

int main(void)
{
    pademu_layout_t l;
    unsigned reg[16], out_reg[16];
    unsigned char in[64], out[64];
    static const unsigned lens[] = {5, 9, 21};
    int a, b, keep, i;

    /* 1. layout of the usual polls: every length pair that adds up */
    for (a = 0; a < 3; a++)
        for (b = 0; b < 3; b++) {
            memset(reg, 0, sizeof(reg));
            reg[0] = REG(0, lens[a]);
            reg[1] = REG(1, lens[b]);
            expect("layout found", pademu_layout_two(reg, lens[a] + lens[b], &l), 1);
            expect("cmd 0 offset", l.off[0], 0);
            expect("cmd 1 offset", l.off[1], lens[a]);
            expect("cmd 1 length", l.len[1], lens[b]);
            expect("cmd 0 port", l.port[0], 0);
            expect("cmd 1 port", l.port[1], 1);
        }

    /* 2. transfers that are not a plain 2 port poll are not split */
    memset(reg, 0, sizeof(reg));
    reg[0] = REG(0, 9);
    expect("one command only", pademu_layout_two(reg, 9, &l), 0);
    reg[1] = REG(1, 9);
    expect("size mismatch", pademu_layout_two(reg, 20, &l), 0);
    reg[2] = REG(0, 9);
    expect("three commands", pademu_layout_two(reg, 27, &l), 0);

    /* 3. the real command goes alone, in its own region of the buffers, and the other one is never sent */
    for (keep = 0; keep < 2; keep++) {
        fake_td real;

        memset(reg, 0, sizeof(reg));
        reg[0] = REG(0, 9);
        reg[1] = REG(1, 21);
        memset(out, 0x11, sizeof(out));
        memset(in, 0x22, sizeof(in));
        expect("layout for split", pademu_layout_two(reg, 30, &l), 1);

        pademu_split_regdata(reg, keep, out_reg);
        expect("kept command is first", out_reg[0], (long)reg[keep]);
        for (i = 1; i < 16; i++)
            expect("queue ends", out_reg[i], 0);

        memcpy(real.regdata, out_reg, sizeof(out_reg));
        real.in_size = l.len[keep];
        real.in = in + l.off[keep];
        real.out = out + l.off[keep];

        hw_commands = 0;
        memset(hw_ports_seen, 0, sizeof(hw_ports_seen));
        fake_sio2(&real);

        expect("hardware runs one command", hw_commands, 1);
        expect("only the real port is used", hw_ports_seen[keep], 1);
        expect("the emulated port is not touched", hw_ports_seen[1 - keep], 0);
        expect("answer of the real pad", out[l.off[keep]], 0xA0 + keep);
        expect("answer lands in its own region", out[l.off[keep] + l.len[keep] - 1], 0xA0 + keep);
        expect("emulated answer area untouched", out[l.off[1 - keep]], 0x11);
    }

    printf("%d checks, %d failures\n", checks, failures);
    return failures != 0;
}
