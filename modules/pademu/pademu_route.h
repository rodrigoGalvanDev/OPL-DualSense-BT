/*
 * PADEMU routing: which controller answers a pad command sent to a SIO2 port.
 *
 * Pure logic with no SDK dependency, so it can be unit tested on a PC (tools/test_pademu_route.c).
 *
 * Terms:
 *  - pad index: the number of the controller on the PADEMU side (Bluetooth or USB pad 0..3).
 *    Without the multitap it is the PS2 port (0 = 1P, 1 = 2P). With the multitap it is the slot (0 = A ... 3 = D).
 *  - "real": the transfer is left to the console's own SIO2 hardware, so the DS2 plugged in the port answers.
 *
 * Without the multitap, a port is emulated when its pad is enabled in the menu.
 *
 * With the multitap, PADEMU emulates the multitap of one port (mtap_port). The other port is never touched.
 *  - The game has not asked for the multitap (mtap_inited == 0): the port behaves as a plain pad, that is slot A.
 *  - The game uses the multitap: the slot it selected last decides.
 *  - Slot A: emulated when enabled in the menu, otherwise the real DS2 plugged in the port.
 *  - Slots B, C, D: always PADEMU. Without a physical multitap there is no real pad that could be there; a slot
 *    without a connected wireless pad answers "no pad".
 */

#ifndef PADEMU_ROUTE_H
#define PADEMU_ROUTE_H

#define PADEMU_ROUTE_REAL (-1)

typedef struct
{
    unsigned char enabled;      // bit n: pad index n is enabled in the menu
    unsigned char mtap_enabled; // multitap emulation switched on in the menu
    unsigned char mtap_port;    // PS2 port (0/1) of the emulated multitap
    unsigned char mtap_inited;  // the game asked the multitap for its slot count
    unsigned char mtap_slot;    // slot the game selected last
} pademu_route_t;

// pad index to emulate for a pad command sent to `port` (0 or 1), or PADEMU_ROUTE_REAL
static inline int pademu_route_port(const pademu_route_t *r, int port)
{
    int slot;

    if (port < 0 || port > 1)
        return PADEMU_ROUTE_REAL;

    if (!r->mtap_enabled)
        return ((r->enabled >> port) & 1) ? port : PADEMU_ROUTE_REAL;

    if (port != r->mtap_port)
        return PADEMU_ROUTE_REAL;

    slot = r->mtap_inited ? (r->mtap_slot & 3) : 0;

    if (slot == 0)
        return (r->enabled & 1) ? 0 : PADEMU_ROUTE_REAL;

    return slot;
}

// true when a multitap command (0x21 device) sent to `port` belongs to the multitap PADEMU emulates.
// Commands for any other port go to the console's SIO2 untouched, so a real multitap there keeps working.
static inline int pademu_route_is_mtap_port(const pademu_route_t *r, int port)
{
    return r->mtap_enabled && port >= 0 && port <= 1 && port == r->mtap_port;
}

#endif
