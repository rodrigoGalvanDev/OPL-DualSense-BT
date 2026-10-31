#include <kernel.h>
#include <sifrpc.h>
#include <string.h>
#include "libds34bt.h"

#define DS34BT_INIT          1
#define DS34BT_INIT_CHARGING 2
#define DS34BT_GET_STATUS    3
#define DS34BT_GET_BDADDR    4
#define DS34BT_SET_RUMBLE    5
#define DS34BT_SET_LED       6
#define DS34BT_GET_DATA      7
#define DS34BT_RESET         8
#define DS34BT_GET_VERSION   9
#define DS34BT_GET_FEATURES  10
#define DS34BT_SEARCH_START  11
#define DS34BT_GET_SEARCH    12
#define DS34BT_PAIR          13
#define DS34BT_GET_PAIR      14
#define DS34BT_SET_KEY       15
#define DS34BT_GET_DEBUG     16
#define DS34BT_GET_PADDEBUG  17
#define DS34BT_GET_INFO_ALL  18
#define DS34BT_DISCONNECT    19
#define DS34BT_GET_PAD_ADDR  20

#define DS34BT_BIND_RPC_ID 0x18E3878F

static SifRpcClientData_t ds34bt;

static u8 rpcbuf[64] __attribute__((aligned(64)));

static u8 ds34bt_inited = 0;

int ds34bt_init()
{
    ds34bt.server = NULL;

    do {
        if (SifBindRpc(&ds34bt, DS34BT_BIND_RPC_ID, 0) < 0)
            return 0;

        nopdelay();
    } while (!ds34bt.server);

    ds34bt_inited = 1;

    return 1;
}

int ds34bt_deinit()
{
    ds34bt_inited = 0;
    return 1;
}

int ds34bt_reinit_ports(u8 ports)
{
    if (!ds34bt_inited)
        return 0;

    rpcbuf[0] = ports;
    return (SifCallRpc(&ds34bt, DS34BT_INIT, 0, rpcbuf, 1, NULL, 0, NULL, NULL) == 0);
}

int ds34bt_init_charging()
{
    if (!ds34bt_inited)
        return 0;

    return (SifCallRpc(&ds34bt, DS34BT_INIT_CHARGING, 0, NULL, 0, NULL, 0, NULL, NULL) == 0);
}

int ds34bt_get_status(int port)
{
    if (!ds34bt_inited)
        return 0;

    rpcbuf[0] = port;

    if (SifCallRpc(&ds34bt, DS34BT_GET_STATUS, 0, rpcbuf, 1, rpcbuf, 1, NULL, NULL) == 0)
        return rpcbuf[0];

    return 0;
}

int ds34bt_get_bdaddr(u8 *bdaddr)
{
    int ret = 0;

    if (!ds34bt_inited)
        return 0;

    if (SifCallRpc(&ds34bt, DS34BT_GET_BDADDR, 0, NULL, 0, rpcbuf, 7, NULL, NULL) == 0) {
        memcpy(bdaddr, rpcbuf, 6);

        ret = rpcbuf[6];
    }

    return ret;
}

int ds34bt_set_rumble(int port, u8 lrum, u8 rrum)
{
    if (!ds34bt_inited)
        return 0;

    rpcbuf[0] = port;
    rpcbuf[1] = lrum;
    rpcbuf[2] = rrum;

    return (SifCallRpc(&ds34bt, DS34BT_SET_RUMBLE, 0, rpcbuf, 3, NULL, 0, NULL, NULL) == 0);
}

int ds34bt_set_led(int port, u8 *led)
{
    if (!ds34bt_inited)
        return 0;

    rpcbuf[0] = port;
    rpcbuf[1] = led[0];
    rpcbuf[2] = led[1];
    rpcbuf[3] = led[2];
    rpcbuf[4] = led[3]; // bit0: blink, bit1: colour chosen by the user (kept by the driver)

    return (SifCallRpc(&ds34bt, DS34BT_SET_LED, 0, rpcbuf, 5, NULL, 0, NULL, NULL) == 0);
}

int ds34bt_get_data(int port, u8 *data)
{
    int ret;

    if (!ds34bt_inited)
        return 0;

    rpcbuf[0] = port;

    ret = (SifCallRpc(&ds34bt, DS34BT_GET_DATA, 0, rpcbuf, 1, rpcbuf, 18, NULL, NULL) == 0);

    memcpy(data, rpcbuf, 18);

    return ret;
}

int ds34bt_reset()
{
    if (!ds34bt_inited)
        return 0;

    return (SifCallRpc(&ds34bt, DS34BT_RESET, 0, NULL, 0, NULL, 0, NULL, NULL) == 0);
}

int ds34bt_get_version(hci_information_t *info)
{
    int ret = 0;

    if (!ds34bt_inited)
        return 0;

    if (SifCallRpc(&ds34bt, DS34BT_GET_VERSION, 0, NULL, 0, rpcbuf, 1 + sizeof(hci_information_t), NULL, NULL) == 0) {
        memcpy(info, rpcbuf, sizeof(hci_information_t));

        ret = rpcbuf[sizeof(hci_information_t)];
    }

    return ret;
}

int ds34bt_get_features(u8 *info)
{
    int ret = 0;

    if (!ds34bt_inited)
        return 0;

    if (SifCallRpc(&ds34bt, DS34BT_GET_FEATURES, 0, NULL, 0, rpcbuf, 9, NULL, NULL) == 0) {
        memcpy(info, rpcbuf, 8);

        ret = rpcbuf[8];
    }

    return ret;
}

int ds34bt_search_start(void)
{
    if (!ds34bt_inited)
        return 0;

    if (SifCallRpc(&ds34bt, DS34BT_SEARCH_START, 0, rpcbuf, 1, rpcbuf, 1, NULL, NULL) == 0)
        return rpcbuf[0];

    return 0;
}

int ds34bt_get_search(int index, int *count, u8 *bdaddr, char *name)
{
    if (!ds34bt_inited)
        return -1;

    rpcbuf[0] = index;

    if (SifCallRpc(&ds34bt, DS34BT_GET_SEARCH, 0, rpcbuf, 1, rpcbuf, 8 + DS34BT_NAME_LEN, NULL, NULL) != 0)
        return -1;

    if (count)
        *count = rpcbuf[1];
    if (bdaddr)
        memcpy(bdaddr, &rpcbuf[2], 6);
    if (name) {
        memcpy(name, &rpcbuf[8], DS34BT_NAME_LEN);
        name[DS34BT_NAME_LEN - 1] = '\0';
    }

    return rpcbuf[0];
}

// Drops the connected pad of this port (it can connect again by pressing PS). Returns 1 if a pad was disconnected.
int ds34bt_disconnect(int port)
{
    if (!ds34bt_inited)
        return 0;

    rpcbuf[0] = port;

    if (SifCallRpc(&ds34bt, DS34BT_DISCONNECT, 0, rpcbuf, 1, rpcbuf, 1, NULL, NULL) == 0)
        return rpcbuf[0];

    return 0;
}

int ds34bt_pair(int index)
{
    if (!ds34bt_inited)
        return 0;

    rpcbuf[0] = index;

    if (SifCallRpc(&ds34bt, DS34BT_PAIR, 0, rpcbuf, 1, rpcbuf, 1, NULL, NULL) == 0)
        return rpcbuf[0];

    return 0;
}

int ds34bt_get_pair(u8 *bdaddr, u8 *key)
{
    if (!ds34bt_inited)
        return -1;

    if (SifCallRpc(&ds34bt, DS34BT_GET_PAIR, 0, NULL, 0, rpcbuf, 23, NULL, NULL) != 0)
        return -1;

    if (bdaddr)
        memcpy(bdaddr, &rpcbuf[1], 6);
    if (key)
        memcpy(key, &rpcbuf[7], 16);

    return rpcbuf[0];
}

int ds34bt_set_key(const u8 *bdaddr, const u8 *key)
{
    if (!ds34bt_inited)
        return 0;

    if (bdaddr && key) {
        rpcbuf[0] = 1;
        memcpy(&rpcbuf[1], bdaddr, 6);
        memcpy(&rpcbuf[7], key, 16);
    } else {
        rpcbuf[0] = 0; // clear all
    }

    return (SifCallRpc(&ds34bt, DS34BT_SET_KEY, 0, rpcbuf, 23, NULL, 0, NULL, NULL) == 0);
}

// Debug info of the iop driver: out = DS34BT_DEBUG_SIZE bytes (layout: DBG_* in modules/ds34bt/iop/ds34bt.c)
int ds34bt_get_debug(u8 *out)
{
    if (!ds34bt_inited)
        return 0;

    if (SifCallRpc(&ds34bt, DS34BT_GET_DEBUG, 0, NULL, 0, rpcbuf, DS34BT_DEBUG_SIZE, NULL, NULL) != 0)
        return 0;

    memcpy(out, rpcbuf, DS34BT_DEBUG_SIZE);

    return 1;
}

// Pad 0 diagnostics of the iop driver: out = DS34BT_DEBUG_SIZE bytes (layout: ds34bt_get_paddebug in modules/ds34bt/iop/ds34bt.c)
int ds34bt_get_paddebug(u8 *out)
{
    if (!ds34bt_inited)
        return 0;

    if (SifCallRpc(&ds34bt, DS34BT_GET_PADDEBUG, 0, NULL, 0, rpcbuf, DS34BT_DEBUG_SIZE, NULL, NULL) != 0)
        return 0;

    memcpy(out, rpcbuf, DS34BT_DEBUG_SIZE);

    return 1;
}

// Connected pad, type and battery of every port: one RPC, the result is kept for ds34bt_cached_info()
static ds34bt_info_t info_cache[DS34BT_PADS_MAX];

int ds34bt_update_info(void)
{
    int i;

    memset(info_cache, 0, sizeof(info_cache));
    for (i = 0; i < DS34BT_PADS_MAX; i++)
        info_cache[i].percent = DS34BT_BAT_PCT_UNKNOWN;

    if (!ds34bt_inited)
        return 0;

    if (SifCallRpc(&ds34bt, DS34BT_GET_INFO_ALL, 0, NULL, 0, rpcbuf, DS34BT_PADS_MAX * 4, NULL, NULL) != 0)
        return 0;

    for (i = 0; i < DS34BT_PADS_MAX; i++) {
        info_cache[i].connected = rpcbuf[i * 4 + 0];
        info_cache[i].type = rpcbuf[i * 4 + 1];
        info_cache[i].percent = rpcbuf[i * 4 + 2];
        info_cache[i].state = rpcbuf[i * 4 + 3];
    }

    return 1;
}

const ds34bt_info_t *ds34bt_cached_info(int port)
{
    return &info_cache[(port >= 0 && port < DS34BT_PADS_MAX) ? port : 0];
}

// Bluetooth address (HCI order) of the pad running on a port. Returns 1 and fills bdaddr[6] when there is one.
int ds34bt_get_pad_addr(int port, u8 *bdaddr)
{
    if (!ds34bt_inited || port < 0 || port >= DS34BT_PADS_MAX)
        return 0;

    rpcbuf[0] = port;
    if (SifCallRpc(&ds34bt, DS34BT_GET_PAD_ADDR, 0, rpcbuf, 1, rpcbuf, 8, NULL, NULL) != 0)
        return 0;

    if (!rpcbuf[6])
        return 0;

    memcpy(bdaddr, rpcbuf, 6);
    return 1;
}
