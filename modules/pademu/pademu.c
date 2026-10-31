// sio2man hook code taken from opl mcemu:
/*
   Copyright 2006-2008, Romz
   Copyright 2010, Polo
   Licenced under Academic Free License version 3.0
   Review OpenUsbLd README & LICENSE files for further details.
*/

#include "pademu.h"
#include "padmacro.h"
#include "pademu_route.h"
#define PADEMU_REG_T u32 // type of sio2_transfer_data_t.regdata
#include "pademu_split.h"

#ifdef BT

#include "ds34bt.h"
#include <padsetup.h>

#define PAD_INIT            ds34bt_init
#define PAD_GET_STATUS      ds34bt_get_status
#define PAD_RESET           ds34bt_reset
#define PAD_GET_DATA        ds34bt_get_data
#define PAD_SET_RUMBLE      ds34bt_set_rumble
#define PAD_SET_MODE        ds34bt_set_mode
#define PAD_GET_MODEL(port) 3

#elif defined(USB)

#include "ds34usb.h"

#define PAD_INIT       ds34usb_init
#define PAD_GET_STATUS ds34usb_get_status
#define PAD_RESET      ds34usb_reset
#define PAD_GET_DATA   ds34usb_get_data
#define PAD_GET_MODEL  ds34usb_get_model
#define PAD_SET_RUMBLE ds34usb_set_rumble
#define PAD_SET_MODE   ds34usb_set_mode

#else
#error "must define mode"
#endif

//#define DPRINTF(x...) printf(x)
#define DPRINTF(x...)

// Routing log, debug builds only (needs the stdio import, see imports.lst): one line each time a decision changes
#ifdef PADEMU_DEBUG
#define PADEMU_LOG(x...) printf(x)
#else
#define PADEMU_LOG(x...)
#endif

typedef struct
{
    u8 mode;
    u8 mode_p;
    u8 mode_id;
    u8 mode_cfg;
    u8 mode_lock;
    u8 enabled;
    u8 vibration;
    u8 lrum;
    u8 rrum;
    u8 mask[4];
} pad_status_t;

#define DIGITAL_MODE 0x41
#define ANALOG_MODE  0x73
#define ANALOGP_MODE 0x79
#define CONFIG_MODE  0xF3

#define MAX_PORTS 4

#define PAD_STATE_RUNNING 0x08

IRX_ID("pademu", 1, 1);

PtrRegisterLibraryEntires pRegisterLibraryEntires; /* Pointer to RegisterLibraryEntires routine */
Sio2McProc pSio2man25[2], pSio2man51[2];           /* Pointers to SIO2MAN routines */
pad_status_t pad[MAX_PORTS];

static u8 pad_inited = 0;
static u8 pad_enable = 0;
static u8 pad_options = 0;

static pademu_route_t pad_route; // which controller answers which port / multitap slot (pademu_route.h)

/*
 * The pad driver (Bluetooth adapter or USB pads) is started by the first pad command, not at module start.
 * It used to start only when a command reached an emulated pad. With the multitap emulated and slot A left to the
 * real controller, the first commands go to the real pad or to the multitap itself, so a wireless pad had nothing
 * to connect to until the game happened to poll slot B or C. It now starts on the first pad or multitap command
 * as soon as any pad is enabled in the menu.
 */
static void pademu_start_pads(void)
{
    if (!pad_inited) {
        pad_inited = PAD_INIT(pad_enable, pad_options);
        PADEMU_LOG("PADEMU: pad driver start for slots %02X: %s\n", pad_enable, pad_inited ? "ok" : "failed, will retry");
    }
}

int install_sio2hook();

int hookRegisterLibraryEntires(iop_library_t *lib);
static void hookSio2man25_1(sio2_transfer_data_t *sd);
static void hookSio2man25_2(sio2_transfer_data_t *sd);
static void hookSio2man51_1(sio2_transfer_data_t *sd);
static void hookSio2man51_2(sio2_transfer_data_t *sd);
void InstallSio2manHook(void *exp, int ver);

void pademu_hookSio2man(sio2_transfer_data_t *td, Sio2McProc sio2proc);
void pademu_setup(u8 ports, u8 vib);
void pademu(sio2_transfer_data_t *td);
void pademu_cmd(int port, u8 *in, u8 *out, u8 out_size);

void pademu_mtap(sio2_transfer_data_t *td);

extern struct irx_export_table _exp_pademu;

int _start(int argc, char *argv[])
{
    union
    {
        struct
        {
            u8 pad_enable;
            u8 pad_vibration;
            u8 mtap_enabled : 1;
            u8 mtap_port    : 1;
            u8 pad_options  : 1;
        };
        int raw;
    } PadEmuSettings_local;
    u8 pad_vibration = 0x03;

    pad_enable = 0x03;

    if (argc > 1) {
        mips_memcpy(&PadEmuSettings_local.raw, argv[1], 4);
        pad_enable = PadEmuSettings_local.pad_enable;
        pad_vibration = PadEmuSettings_local.pad_vibration;
        pad_route.mtap_enabled = PadEmuSettings_local.mtap_enabled;
        pad_route.mtap_port = PadEmuSettings_local.mtap_port;
        pad_options = PadEmuSettings_local.pad_options; // enable workaround for fake ds3
        PADEMU_LOG("PADEMU: args: emulated slots %02X, vibration %02X, multitap %d on port %d, fake DS3 workaround %d\n", pad_enable,
                   pad_vibration, pad_route.mtap_enabled, pad_route.mtap_port + 1, pad_options);
        u32 macro_settings = 0xAB;
        mips_memcpy(&macro_settings, argv[1] + 4, 4);
        padMacroInit(macro_settings);
#ifdef BT
        ds34bt_set_setup(argv[1] + 8);                   // Controller Setup blob (include/padsetup.h), PADSETUP_ARG_LEN bytes
        ds34bt_set_keys(argv[1] + 8 + PADSETUP_ARG_LEN); // optional: link keys of pads paired wirelessly
#endif
    }

    if (RegisterLibraryEntries(&_exp_pademu) != 0) {
        return MODULE_NO_RESIDENT_END;
    }

    SetRebootTimeLibraryHandlingMode(&_exp_pademu, 2);

    u8 vmc = 0;

    if (argc > 1)
        vmc = argv[1][3];

    if (!vmc) {
        if (!install_sio2hook())
            return MODULE_NO_RESIDENT_END;
    }

    pademu_setup(pad_enable, pad_vibration);

    return MODULE_RESIDENT_END;
}

void _exit(int mode)
{
    PAD_RESET();
}

int install_sio2hook()
{
    register void *exp;

    /* looking for LOADCORE's library entry table */
    exp = GetExportTable("loadcore", 0x100);
    if (exp == NULL) {
        DPRINTF("Unable to find loadcore exports.\n");
        return 0;
    }

    /* hooking LOADCORE's RegisterLibraryEntires routine */
    pRegisterLibraryEntires = (PtrRegisterLibraryEntires)HookExportEntry(exp, 6, hookRegisterLibraryEntires);

    /* searching for a SIO2MAN export table */
    exp = GetExportTable("sio2man", 0x100);
    if (exp != NULL) {
        /* hooking SIO2MAN's routines */
        InstallSio2manHook(exp, 0);
    } else {
        DPRINTF("SIO2MAN V1 exports not found.\n");
    }

    /* searching for a SIO2MAN export table */
    exp = GetExportTable("sio2man", 0x200);
    if (exp != NULL) {
        /* hooking SIO2MAN's routines */
        InstallSio2manHook(exp, 1);
    } else {
        DPRINTF("SIO2MAN V2 exports not found.\n");
    }

    return 1;
}

void InstallSio2manHook(void *exp, int ver)
{
    /* hooking SIO2MAN entry #25 (used by MCMAN and old PADMAN) */
    pSio2man25[ver] = HookExportEntry(exp, 25, ver ? hookSio2man25_2 : hookSio2man25_1);
    /* hooking SIO2MAN entry #51 (used by MC2_* modules and PADMAN) */
    pSio2man51[ver] = HookExportEntry(exp, 49 + (ver * 2), ver ? hookSio2man51_2 : hookSio2man51_1);
    if (pSio2man51[ver] == (ver ? hookSio2man25_2 : hookSio2man25_1))
        pSio2man51[ver] = pSio2man25[ver];
}

/* Hook for the LOADCORE's RegisterLibraryEntires call */
int hookRegisterLibraryEntires(iop_library_t *lib)
{
    register int ret;

    if (!memcmp(lib->name, "sio2man", 8)) {
        ret = pRegisterLibraryEntires(lib);
        if (ret == 0) {
            ReleaseLibraryEntries((struct irx_export_table *)lib);
            /* hooking SIO2MAN's routines */
            InstallSio2manHook(&lib[1], GetExportTableSize(&lib[1]) >= 61);
        } else {
            DPRINTF("registering library %s failed, error %d\n", lib->name, ret);
            return ret;
        }
    }

    DPRINTF("registering library %s\n", lib->name);

    return pRegisterLibraryEntires(lib);
}

/* Hook for SIO2MAN V2 entry #25 */
static void hookSio2man25_1(sio2_transfer_data_t *sd)
{
    pademu_hookSio2man(sd, pSio2man25[0]);
}

/* Hook for SIO2MAN V2 entry #25 */
static void hookSio2man25_2(sio2_transfer_data_t *sd)
{
    pademu_hookSio2man(sd, pSio2man25[1]);
}

/* Hook for SIO2MAN V2 entry #51 */
static void hookSio2man51_1(sio2_transfer_data_t *sd)
{
    pademu_hookSio2man(sd, pSio2man51[0]);
}

/* Hook for SIO2MAN V2 entry #51 */
static void hookSio2man51_2(sio2_transfer_data_t *sd)
{
    pademu_hookSio2man(sd, pSio2man51[1]);
}

// Offset of the second command of a transfer that polls port 1 and port 2 together, 0 when it can't be found.
// The command lengths in regdata give it exactly; the byte scan is only the fallback for transfers they do not describe.
static int pademu_second_cmd(const sio2_transfer_data_t *td)
{
    pademu_layout_t layout;
    u32 off;

    if (pademu_layout_two(td->regdata, td->in_size, &layout))
        return (int)layout.off[1];

    for (off = 5; off < td->in_size - 3; off++) {
        if (td->in[off] == 0x01 && (td->in[off + 1] & 0xF0) == 0x40 && td->in[off + 2] == 0x00) {
            if (off != 5 && off != 9 && off != 21)
                continue;
            else
                break;
        }
    }

    if (off + 3 >= td->in_size) {
        PADEMU_LOG("PADEMU: second command not found\n");
        return 0;
    }

    return (int)off;
}

#ifdef PADEMU_DEBUG
static void pademu_log_route(int both_ports, int port, int pad_idx)
{
    static u8 seen[2][2]; // bit per route (real, pad 0..3): the route changes with every multitap slot, log each one once

    if (!(seen[both_ports][port] & (1 << (pad_idx + 1)))) {
        seen[both_ports][port] |= 1 << (pad_idx + 1);
        if (pad_idx == PADEMU_ROUTE_REAL)
            PADEMU_LOG("PADEMU: %s port %d -> real DS2\n", both_ports ? "2-port poll" : "poll", port + 1);
        else
            PADEMU_LOG("PADEMU: %s port %d -> emulated pad %d\n", both_ports ? "2-port poll" : "poll", port + 1, pad_idx);
    }
}
#else
#define pademu_log_route(both_ports, port, pad_idx)
#endif

// 2 port poll where one port is a real controller and the other one is emulated.
// The real command is sent to the hardware as a transfer of its own: same buffers, only that command in the queue, so
// no device is addressed that is not there. The emulated command is then answered in software.
static void pademu_poll_mixed(sio2_transfer_data_t *td, Sio2McProc sio2proc, int pad1, int pad2, int cmd2)
{
    pademu_layout_t layout;
    int real_idx = (pad1 == PADEMU_ROUTE_REAL) ? 0 : 1; // command that stays with the real controller
    int emu_pad = (real_idx == 0) ? pad2 : pad1;

#ifdef PADEMU_DEBUG
    {
        static int last_how = -1; // log once, and again if the way changes
        int how = pademu_layout_two(td->regdata, td->in_size, &layout);

        if (how != last_how) {
            last_how = how;
            PADEMU_LOG("PADEMU: mixed poll: %s (regdata %08X %08X, size %d)\n", how ? "real command sent alone" : "fallback", (unsigned)td->regdata[0],
                       (unsigned)td->regdata[1], (int)td->in_size);
        }
    }
#endif

    if (pademu_layout_two(td->regdata, td->in_size, &layout)) {
        sio2_transfer_data_t real;

        mips_memcpy(&real, td, sizeof(real)); // not "real = *td": that makes the compiler call memcpy, which the module does not import

        pademu_split_regdata(td->regdata, real_idx, real.regdata);
        real.in_size = layout.len[real_idx];
        real.out_size = layout.len[real_idx];
        real.in = td->in + layout.off[real_idx];
        real.out = td->out + layout.off[real_idx];

        sio2proc(&real);
    } else {
        // not a transfer the command lengths describe: keep the old way, the emulated command's device is switched off
        int off = (real_idx == 0) ? cmd2 : 0;

        td->in[off] = 0x00;
        sio2proc(td);
        td->in[off] = 0x01;
    }

    pademu_start_pads();

    if (real_idx == 0)
        pademu_cmd(emu_pad, &td->in[cmd2], &td->out[cmd2], td->in_size - cmd2);
    else
        pademu_cmd(emu_pad, td->in, td->out, cmd2);

    td->stat6c = 0x1100;
    td->stat70 = 0x0F;
}

void pademu_hookSio2man(sio2_transfer_data_t *td, Sio2McProc sio2proc)
{
    register u32 ctrl, port1, port2;

    ctrl = td->regdata[0];
    port1 = td->regdata[0] & 0x03;
    port2 = td->regdata[1] & 0x03;

    if ((ctrl & 0xF0) == 0x40) {
        // Any emulated option is on: bring the pad driver up on the first pad or multitap command
        if (pad_route.enabled || pad_route.mtap_enabled) {
            if (td->in[0] == 0x01 || td->in[0] == 0x21)
                pademu_start_pads();
        }

#ifdef PADEMU_DEBUG
        if (td->in[0] == 0x21) { // multitap command: say once per port and result whether PADEMU answers it
            static u8 mtap_seen[2][2];
            int match = (td->port_ctrl2[port1] == 0x00030064) && pademu_route_is_mtap_port(&pad_route, (int)(port1 & 1));

            if (!mtap_seen[port1 & 1][match]) {
                mtap_seen[port1 & 1][match] = 1;
                PADEMU_LOG("PADEMU: multitap command on port %d (port_ctrl2 %08X): %s\n", (int)(port1 & 1) + 1, (unsigned)td->port_ctrl2[port1],
                           match ? "answered by PADEMU" : "left to the console");
            }
        }
#endif
        if (td->port_ctrl2[port1] == 0x00030064 && td->in[0] == 0x21 && pademu_route_is_mtap_port(&pad_route, (int)(port1 & 1))) {
            // MTAPMAN addresses the multitap of PS2 port N as SIO2 port N|2 (regdata port 2 or 3), so port1 is 2 or 3 here
            sio2proc = pademu_mtap;
        } else if (td->in[0] == 0x01) {
            if (port2 == 1) { // 2 sio cmds: port 1 and port 2 polled together
                int cmd2 = pademu_second_cmd(td);

                if (cmd2) {
                    int pad1 = pademu_route_port(&pad_route, 0);
                    int pad2 = pademu_route_port(&pad_route, 1);

                    pademu_log_route(1, 0, pad1);
                    pademu_log_route(1, 1, pad2);

                    if (pad1 != PADEMU_ROUTE_REAL && pad2 != PADEMU_ROUTE_REAL) { // both emulated
                        sio2proc = pademu;
                    } else if (pad1 != PADEMU_ROUTE_REAL || pad2 != PADEMU_ROUTE_REAL) {
                        // One real pad and one emulated. The real command goes alone through the console's SIO2
                        // (pademu_poll_mixed), the emulated one is answered here.
                        pademu_poll_mixed(td, sio2proc, pad1, pad2, cmd2);
                        return;
                    }
                }
            } else {
                int pad_idx = pademu_route_port(&pad_route, port1);

                pademu_log_route(0, port1 & 1, pad_idx);

                if (pad_idx != PADEMU_ROUTE_REAL)
                    sio2proc = pademu;
            }
        }
    }

    sio2proc(td);
}

void pademu_setup(u8 ports, u8 vib)
{
    u8 i;

    pad_route.enabled = ports & 0x0F;

    for (i = 0; i < MAX_PORTS; i++) {
        pad[i].mode = 0;
        pad[i].mode_p = 0;
        pad[i].mode_id = DIGITAL_MODE;
        pad[i].mode_cfg = 0;
        pad[i].mode_lock = 0;

        pad[i].enabled = ((ports >> i) & 1);
        pad[i].vibration = ((vib >> i) & 1);

        pad[i].mask[0] = 0xFF;
        pad[i].mask[1] = 0xFF;
        pad[i].mask[2] = 0x03;
        pad[i].mask[3] = 0x00;

        pad[i].lrum = 2;
        pad[i].rrum = 2;
    }
}

u8 pademu_data[6][6] =
    {
        {0x00, 0x00, 0x02, 0x00, 0x00, 0x5A},
        {0x03, 0x02, 0x00, 0x02, 0x01, 0x00},
        {0x00, 0x00, 0x01, 0x02, 0x00, 0x0A},
        {0x00, 0x00, 0x01, 0x01, 0x01, 0x14},
        {0x00, 0x00, 0x02, 0x00, 0x01, 0x00},
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

void pademu(sio2_transfer_data_t *td)
{
    u8 port1, port2;
    int cmd2, pad1, pad2;

    port1 = td->regdata[0] & 0x03;
    port2 = td->regdata[1] & 0x03;

    td->stat6c = 0x1100; //?
    td->stat70 = 0x0F;   //?

    pademu_start_pads();

    if (port2 == 1) { // port 1 and port 2 polled together
        cmd2 = pademu_second_cmd(td);
        if (!cmd2)
            return;

        pad1 = pademu_route_port(&pad_route, 0);
        pad2 = pademu_route_port(&pad_route, 1);

        if (pad2 != PADEMU_ROUTE_REAL)
            pademu_cmd(pad2, &td->in[cmd2], &td->out[cmd2], td->in_size - cmd2);
        if (pad1 != PADEMU_ROUTE_REAL)
            pademu_cmd(pad1, td->in, td->out, cmd2);
    } else {
        pad1 = pademu_route_port(&pad_route, port1);

        if (pad1 != PADEMU_ROUTE_REAL)
            pademu_cmd(pad1, td->in, td->out, td->in_size);
    }
}

void pademu_cmd(int port, u8 *in, u8 *out, u8 out_size)
{
    u8 i;

    mips_memset(out, 0x00, out_size);

#ifdef PADEMU_DEBUG
    {
        static signed char was_running[4] = {-1, -1, -1, -1}; // one line when a pad becomes ready or goes away
        int running = (PAD_GET_STATUS(port) & PAD_STATE_RUNNING) ? 1 : 0;

        if (port >= 0 && port < 4 && was_running[port] != running) {
            was_running[port] = running;
            PADEMU_LOG("PADEMU: emulated pad %d: %s\n", port, running ? "controller connected" : "no controller");
        }
    }
#endif

    if (!(PAD_GET_STATUS(port) & PAD_STATE_RUNNING)) {
        pad[port].lrum = 2;
        pad[port].rrum = 2;
        return;
    }

    out[0] = 0xFF;
    out[1] = CONFIG_MODE;
    out[2] = 0x5A;

    switch (in[1]) {
        case 0x40: // set vref param
            mips_memcpy(&out[3], &pademu_data[0], 6);
            break;

        case 0x41: // query button mask
            if (pad[port].mode_id != DIGITAL_MODE) {
                out[3] = pad[port].mask[0];
                out[4] = pad[port].mask[1];
                out[5] = pad[port].mask[2];
                out[6] = pad[port].mask[3];
                out[7] = 0x00;
                out[8] = 0x5A;
            }
            break;

        case 0x43: // enter/exit config mode
            if (pad[port].mode_cfg) {
                pad[port].mode_cfg = in[3];
                break;
            }

            pad[port].mode_cfg = in[3];
        case 0x42: // read data
            if (in[1] == 0x42) {
                if (pad[port].vibration) { // disable/enable vibration
                    PAD_SET_RUMBLE(in[pad[port].lrum], in[pad[port].rrum], port);
                }
            }

            i = PAD_GET_DATA(&out[3], out_size - 3, port);

            if (pad[port].mode_lock == 0) { // mode unlocked
                if (pad[port].mode != i) {
                    pad[port].mode = i;

                    if (pad[port].mode)
                        pad[port].mode_id = ANALOG_MODE;
                    else
                        pad[port].mode_id = DIGITAL_MODE;
                }
            }

            out[1] = pad[port].mode_id;
            break;

        case 0x44: // set mode and lock
            pad[port].mode = in[3];
            pad[port].mode_lock = in[4];
            if (pad[port].mode) {
                if (pad[port].mode_p) {
                    pad[port].mode_id = ANALOGP_MODE;
                } else {
                    pad[port].mode_id = ANALOG_MODE;
                }
            } else {
                pad[port].mode_id = DIGITAL_MODE;
            }
            PAD_SET_MODE(pad[port].mode, pad[port].mode_lock, port);
            break;

        case 0x45: // query model and mode
            mips_memcpy(&out[3], &pademu_data[1], 6);
            out[5] = pad[port].mode;
            out[3] = PAD_GET_MODEL(port);
            break;

        case 0x46: // query act
            if (in[3] == 0x00)
                mips_memcpy(&out[3], &pademu_data[2], 6);
            else
                mips_memcpy(&out[3], &pademu_data[3], 6);

            break;

        case 0x47: // query comb
            mips_memcpy(&out[3], &pademu_data[4], 6);
            break;

        case 0x4C: // query mode
            if (in[3] == 0x00)
                out[6] = 0x04;
            else
                out[6] = 0x07;

            break;

        case 0x4D: // set act align
            mips_memcpy(&out[3], &pademu_data[5], 6);

            for (i = 0; i < 6; i++) { // vibration
                if (in[3 + i] == 0x00)
                    pad[port].rrum = i + 3;

                if (in[3 + i] == 0x01)
                    pad[port].lrum = i + 3;
            }
            break;

        case 0x4F: // set button info
            pad[port].mode_id = ANALOGP_MODE;
            pad[port].mode_p = 1;

            out[8] = 0x5A;

            pad[port].mask[0] = in[3];
            pad[port].mask[1] = in[4];
            pad[port].mask[2] = in[5];
            pad[port].mask[3] = in[6];
            break;
    }
}

static u8 mtap_data[] = {
    0xff, 0x80, 0x5a, 0x00, 0x00, 0x5a};

#define MAX_SLOT 4

void pademu_mtap(sio2_transfer_data_t *td)
{
    u8 port1;

    port1 = td->regdata[0] & 0x01;

    mips_memset(td->out, 0x00, td->out_size);
    td->stat6c = 0x0001d100;

    if (port1 != pad_route.mtap_port) {
        return;
    }

    switch (td->in[1]) {
        case 0x12: // returns slot number for pad
            mips_memcpy(td->out, &mtap_data, sizeof(mtap_data));
            td->out[3] = MAX_SLOT;
            td->stat6c = 0x00001100;
            if (!pad_route.mtap_inited)
                PADEMU_LOG("PADEMU: the game uses the multitap of port %d\n", pad_route.mtap_port + 1);
            pad_route.mtap_inited = 1;
            break;

        case 0x13: // returns slot number for mc
            break;

        case 0x21: // changes slot for pad
            mips_memcpy(td->out, &mtap_data, sizeof(mtap_data));
#ifdef PADEMU_DEBUG
            {
                static u8 slot_seen; // each slot is selected hundreds of times per second: log only the first time

                if (!(slot_seen & (1 << (td->in[2] & 3)))) {
                    slot_seen |= 1 << (td->in[2] & 3);
                    PADEMU_LOG("PADEMU: multitap slot %d\n", td->in[2] & 3);
                }
            }
#endif
            pad_route.mtap_slot = td->out[5] = td->in[2]; // slot
            td->out[6] = 0x5a;
            td->stat6c = 0x00001100;
            break;

        case 0x22: // changes slot for mc
            break;
    }
}
