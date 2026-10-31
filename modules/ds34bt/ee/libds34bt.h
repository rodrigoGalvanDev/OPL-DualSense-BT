
enum eDS34BTStatus {
    DS34BT_STATE_USB_DISCONNECTED = 0x00,
    DS34BT_STATE_USB_AUTHORIZED = 0x01,
    DS34BT_STATE_USB_CONFIGURED = 0x02,
    DS34BT_STATE_CONNECTED = 0x04,
    DS34BT_STATE_RUNNING = 0x08,
};

enum eDS34BTSearch {
    DS34BT_SEARCH_IDLE = 0,
    DS34BT_SEARCH_INQUIRY,
    DS34BT_SEARCH_NAMES,
    DS34BT_SEARCH_DONE,
    DS34BT_SEARCH_ERROR,
};

enum eDS34BTPair {
    DS34BT_PAIR_IDLE = 0,
    DS34BT_PAIR_CONNECTING,
    DS34BT_PAIR_AUTHENTICATING,
    DS34BT_PAIR_ENCRYPTING,
    DS34BT_PAIR_DONE,
    DS34BT_PAIR_FAILED,
};

#define DS34BT_PADS_MAX   4 // Bluetooth pads connected at once
#define DS34BT_SEARCH_MAX 4
#define DS34BT_NAME_LEN   32
#define DS34BT_DEBUG_SIZE 32

typedef struct
{
    u8 hci_ver;     // Version of the Current HCI in the BR/EDR Controller
    u16 hci_rev;    // Revision of the Current HCI in the BR/EDR Controller
    u8 lmp_ver;     // Version of the Current LMP or PAL in the Controller
    u16 mf_name;    // Manufacturer Name of the BR/EDR Controller
    u16 lmp_subver; // Subversion of the Current LMP or PAL in the Controller
    u16 vid;
    u16 pid;
    u16 rev;
} __attribute__((packed)) hci_information_t;

int ds34bt_init();
int ds34bt_deinit();
int ds34bt_reinit_ports(u8 ports);
int ds34bt_init_charging();
int ds34bt_get_status(int port);
int ds34bt_get_bdaddr(u8 *bdaddr);
int ds34bt_set_rumble(int port, u8 lrum, u8 rrum);
int ds34bt_set_led(int port, u8 *led); // r, g, b, flags (4 bytes): bit0 blink, bit1 keep this colour
int ds34bt_get_data(int port, u8 *data);

// Pad type / battery for the menu indicator
enum eDS34BTType {
    DS34BT_TYPE_NONE = 0,
    DS34BT_TYPE_DS3,
    DS34BT_TYPE_DS4,
    DS34BT_TYPE_DS5,
};

enum eDS34BTBattery {
    DS34BT_BAT_UNKNOWN = 0,
    DS34BT_BAT_DISCHARGING,
    DS34BT_BAT_CHARGING,
    DS34BT_BAT_FULL,
};

#define DS34BT_BAT_PCT_UNKNOWN 0xFF

typedef struct
{
    u8 connected; // pad of this port is connected and running
    u8 type;      // eDS34BTType
    u8 percent;   // 0-100, DS34BT_BAT_PCT_UNKNOWN when the pad doesn't tell
    u8 state;     // eDS34BTBattery
} ds34bt_info_t;

int ds34bt_update_info(void);                      // one RPC refreshes the info of every port, returns 1 if the driver answered
const ds34bt_info_t *ds34bt_cached_info(int port); // result of the last ds34bt_update_info, no RPC
int ds34bt_get_pad_addr(int port, u8 *bdaddr);     // BD_ADDR (HCI order) of the pad on a port, 1 if one is running there
int ds34bt_reset();
int ds34bt_get_version(hci_information_t *info);
int ds34bt_get_features(u8 *info);

// Wireless search and Simple Pairing (DS4 / DualSense in pairing mode)
int ds34bt_search_start(void);
int ds34bt_get_search(int index, int *count, u8 *bdaddr, char *name); // returns the search state
int ds34bt_pair(int index);
int ds34bt_disconnect(int port); // drops the pad connected on this port, 1 if there was one
int ds34bt_get_pair(u8 *bdaddr, u8 *key); // returns the pairing state
int ds34bt_set_key(const u8 *bdaddr, const u8 *key); // NULL, NULL clears the table
int ds34bt_get_debug(u8 *out);                       // DS34BT_DEBUG_SIZE bytes of driver status, 1 if read
int ds34bt_get_paddebug(u8 *out);                    // DS34BT_DEBUG_SIZE bytes of pad 0 status (Controller Test), 1 if read
