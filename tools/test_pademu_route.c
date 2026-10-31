/*
 * Host test of the PADEMU routing rules (modules/pademu/pademu_route.h).
 *   gcc -Wall -Wextra -o test_pademu_route tools/test_pademu_route.c && ./test_pademu_route
 */
#include <stdio.h>
#include "../modules/pademu/pademu_route.h"

static int failures;
static int checks;

static void expect(const char *what, int got, int want)
{
    checks++;
    if (got != want) {
        failures++;
        printf("FAIL: %s: got %d, want %d\n", what, got, want);
    }
}

/* the decision the module had before the multitap work, for the setups that do not use the multitap */
static int legacy_plain(unsigned char enabled, int port)
{
    return ((enabled >> port) & 1) ? port : PADEMU_ROUTE_REAL;
}

int main(void)
{
    pademu_route_t r;
    int en, port, slot, mport, inited;

    /* 1. multitap off: the same as the original behaviour, for every combination */
    for (en = 0; en < 16; en++)
        for (port = 0; port < 2; port++) {
            r = (pademu_route_t) {.enabled = (unsigned char)en, .mtap_enabled = 0};
            expect("plain", pademu_route_port(&r, port), legacy_plain((unsigned char)en, port));
        }

    /* 2. ports above 1 are never pad ports */
    r = (pademu_route_t) {.enabled = 0x0F, .mtap_enabled = 0};
    expect("port 2", pademu_route_port(&r, 2), PADEMU_ROUTE_REAL);
    expect("port 3", pademu_route_port(&r, 3), PADEMU_ROUTE_REAL);
    r.mtap_enabled = 1;
    expect("mtap port 2", pademu_route_port(&r, 2), PADEMU_ROUTE_REAL);

    /* 3. multitap on, for both ports and all enabled masks */
    for (mport = 0; mport < 2; mport++)
        for (en = 0; en < 16; en++)
            for (inited = 0; inited < 2; inited++)
                for (slot = 0; slot < 4; slot++) {
                    int other = 1 - mport;

                    r = (pademu_route_t) {.enabled = (unsigned char)en, .mtap_enabled = 1, .mtap_port = (unsigned char)mport, .mtap_inited = (unsigned char)inited, .mtap_slot = (unsigned char)slot};

                    /* the other port always stays with the real DS2, whatever the slot bits say */
                    expect("other port is real", pademu_route_port(&r, other), PADEMU_ROUTE_REAL);

                    int s = inited ? slot : 0;
                    int got = pademu_route_port(&r, mport);

                    if (s == 0)
                        expect("slot A", got, (en & 1) ? 0 : PADEMU_ROUTE_REAL);
                    else
                        expect("slot B-D always emulated", got, s);
                }

    /* 4. the case of the report: multitap on port 1, slot A not emulated, B-D wireless */
    r = (pademu_route_t) {.enabled = 0x0E, .mtap_enabled = 1, .mtap_port = 0, .mtap_inited = 1, .mtap_slot = 0};
    expect("A real DS2", pademu_route_port(&r, 0), PADEMU_ROUTE_REAL);
    expect("2P real DS2", pademu_route_port(&r, 1), PADEMU_ROUTE_REAL);
    r.mtap_slot = 1;
    expect("B wireless", pademu_route_port(&r, 0), 1);
    r.mtap_slot = 3;
    expect("D wireless", pademu_route_port(&r, 0), 3);
    r.mtap_slot = 0;

    /* same on port 2: the DS2 of 1P must not be affected (the old code neutralised the wrong command of a two-port poll) */
    r = (pademu_route_t) {.enabled = 0x0E, .mtap_enabled = 1, .mtap_port = 1, .mtap_inited = 1, .mtap_slot = 2};
    expect("mtap on 2P: 1P real", pademu_route_port(&r, 0), PADEMU_ROUTE_REAL);
    expect("mtap on 2P: slot C", pademu_route_port(&r, 1), 2);

    /* 5. game that never asks for the multitap: slot A is a plain pad, the other port is real */
    r = (pademu_route_t) {.enabled = 0x0F, .mtap_enabled = 1, .mtap_port = 0, .mtap_inited = 0, .mtap_slot = 2};
    expect("no mtap game: 1P is slot A", pademu_route_port(&r, 0), 0);
    expect("no mtap game: 2P real", pademu_route_port(&r, 1), PADEMU_ROUTE_REAL);

    /* 6. multitap commands: only the emulated multitap port is intercepted, the other port reaches the real SIO2 */
    r = (pademu_route_t) {.mtap_enabled = 0, .mtap_port = 0};
    expect("mtap off: no interception p0", pademu_route_is_mtap_port(&r, 0), 0);
    expect("mtap off: no interception p1", pademu_route_is_mtap_port(&r, 1), 0);
    for (mport = 0; mport < 2; mport++) {
        r = (pademu_route_t) {.mtap_enabled = 1, .mtap_port = (unsigned char)mport};
        expect("mtap port intercepted", pademu_route_is_mtap_port(&r, mport), 1);
        expect("other port passes through", pademu_route_is_mtap_port(&r, 1 - mport), 0);
        expect("port 2 passes through", pademu_route_is_mtap_port(&r, 2), 0);
        expect("port 3 passes through", pademu_route_is_mtap_port(&r, 3), 0);
    }

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
