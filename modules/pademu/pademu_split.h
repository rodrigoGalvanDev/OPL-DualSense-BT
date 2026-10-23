/*
 * PADEMU: layout of a SIO2 transfer that polls the two ports together, and how to take one of its commands out.
 *
 * Pure logic with no SDK dependency, so it can be unit tested on a PC (tools/test_pademu_split.c).
 *
 * A SIO2 transfer is a queue of commands (regdata[]). Every entry is independent of the others and is
 * described by itself (this is how the hardware, and PCSX2's SIO2 model, read it):
 *   bits 0-1    port the command is sent to
 *   bits 8-16   length of the command in bytes (the answer has the same length)
 *   length 0    end of the queue
 * The command bytes (td->in) and the answers (td->out) of all the entries are stored one after the other.
 *
 * When one port is emulated and the other is a real controller, the real one must still go through the
 * console's SIO2. The old way ran the whole transfer with the emulated command's device byte changed to 0, so the
 * hardware spent the transfer talking to a device that does not exist. The real command is now sent alone, as a
 * transfer of its own that points into the same buffers, and the emulated command is answered in software.
 */

#ifndef PADEMU_SPLIT_H
#define PADEMU_SPLIT_H

#define PADEMU_CMD_LEN_MASK 0x1FF

// Type of one regdata entry: the SIO2 transfer uses u32, which is not `unsigned` on the IOP toolchain.
// The module defines PADEMU_REG_T as u32 before including this file; the PC test uses the default.
#ifndef PADEMU_REG_T
#define PADEMU_REG_T unsigned
#endif

typedef struct
{
    unsigned off[2]; // offset of each command in td->in and of its answer in td->out
    unsigned len[2]; // length of each command
    unsigned port[2];
} pademu_layout_t;

static inline unsigned pademu_cmd_len(PADEMU_REG_T regdata)
{
    return (regdata >> 8) & PADEMU_CMD_LEN_MASK;
}

// Reads the first two commands of a transfer. Returns 1 only when they describe the whole transfer
// (a third command, or lengths that do not add up to in_size, mean the transfer is not a plain 2 port poll).
static inline int pademu_layout_two(const PADEMU_REG_T *regdata, unsigned in_size, pademu_layout_t *l)
{
    unsigned len0 = pademu_cmd_len(regdata[0]);
    unsigned len1 = pademu_cmd_len(regdata[1]);

    if (len0 == 0 || len1 == 0)
        return 0;
    if (pademu_cmd_len(regdata[2]) != 0)
        return 0;
    if (len0 + len1 != in_size)
        return 0;

    l->off[0] = 0;
    l->len[0] = len0;
    l->port[0] = regdata[0] & 3;
    l->off[1] = len0;
    l->len[1] = len1;
    l->port[1] = regdata[1] & 3;

    return 1;
}

// Fills `regdata_out` (16 entries) with the queue that holds only command `keep` (0 or 1) of the transfer
static inline void pademu_split_regdata(const PADEMU_REG_T *regdata, int keep, PADEMU_REG_T *regdata_out)
{
    int i;

    regdata_out[0] = regdata[keep];
    for (i = 1; i < 16; i++)
        regdata_out[i] = 0;
}

#endif
