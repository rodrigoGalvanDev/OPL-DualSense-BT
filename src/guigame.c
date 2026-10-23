/*
 Per Game Integration by BatRastard
 PADEMU by belek666
*/

#include "include/opl.h"
#include "include/gui.h"
#include "include/ioman.h"
#include "include/lang.h"
#include "include/pad.h"
#include "include/config.h"
#include "include/ethsupport.h"
#include "include/compatupd.h"
#include "include/cheatman.h"
#include "include/system.h"
#include "include/guigame.h"
#include "include/ds34common.h"
#include "include/vmc_groups.h"

#ifdef PADEMU
#include <libds34bt.h>
#include <libds34usb.h>
#include "include/btkeys.h"
#include "include/padsetup.h"
#endif

#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h> // fileXioDevctl("genvmc:", ***)
#include <delaythread.h>

static int configSourceID;
static int dmaMode;
static int compatMode;

static int EnableGSM;
static int GSMVMode;
static int GSMXOffset;
static int GSMYOffset;
static int GSMFIELDFix;

static int EnableCheat;
static int CheatMode;

static int forceGlobalOSDLanguage;

#ifdef PADEMU
static int EnablePadEmu;
static int PadEmuSettings;
static union
{
    struct
    {
        unsigned int left_stick_slowdown  : 4;
        unsigned int right_stick_slowdown : 4;
        unsigned int l_slowdown_enable    : 1;
        unsigned int l_slowdown_toggle    : 1;
        unsigned int r_slowdown_enable    : 1;
        unsigned int r_slowdown_toggle    : 1;
        unsigned int lx_invert            : 1;
        unsigned int ly_invert            : 1;
        unsigned int rx_invert            : 1;
        unsigned int ry_invert            : 1;
        unsigned int turbo_speed          : 2;
    };
    int raw;
} PadMacroSettings;
#endif

static char hexid[32];
static char altStartup[32];
static char vmc1[32];
static char vmc2[32];
static char hexDiscID[15];
static char configSource[128];

// forward declarations.
static void guiGameLoadGSMConfig(config_set_t *configSet, config_set_t *configGame);
static void guiGameLoadCheatsConfig(config_set_t *configSet, config_set_t *configGame);
#ifdef PADEMU
static void guiGameLoadPadEmuConfig(config_set_t *configSet, config_set_t *configGame);
static void guiGameLoadPadMacroConfig(config_set_t *configSet, config_set_t *configGame);
#endif
static int guiGameSaveOSDLanguageGameConfig(config_set_t *configSet, int result);
static void guiGameLoadOSDLanguageConfig(config_set_t *configSet, config_set_t *configGame);

int guiGameAltStartupNameHandler(char *text, int maxLen)
{
    int i;

    int result = diaShowKeyb(text, maxLen, 0, NULL);
    if (result) {
        for (i = 0; text[i]; i++) {
            if (text[i] > 96 && text[i] < 123)
                text[i] -= 32;
        }
    }

    return result;
}

char *gameConfigSource(void)
{
    char *source = configSource;
    return source;
}

// VMC
typedef struct
{                   // size = 76
    int VMC_status; // 0=available, 1=busy
    int VMC_error;
    int VMC_progress;
    char VMC_msg[64];
} statusVMCparam_t;

#define OPERATION_CREATE   0
#define OPERATION_CREATING 1
#define OPERATION_ABORTING 2
#define OPERATION_ENDING   3
#define OPERATION_END      4

static short vmc_refresh;
static int vmc_operation;
static statusVMCparam_t vmc_status;

int guiGameVmcNameHandler(char *text, int maxLen)
{
    int result = diaShowKeyb(text, maxLen, 0, NULL);

    if (result)
        vmc_refresh = 1;

    return result;
}

static int guiGameRefreshVMCConfig(item_list_t *support, char *name)
{
    int size = support->itemCheckVMC(support, name, 0);

    if (size != -1) {
        diaSetLabel(diaVMC, VMC_STATUS, _l(_STR_VMC_FILE_EXISTS));

        if (size == 8)
            diaSetInt(diaVMC, VMC_SIZE, 0);
        else if (size == 16)
            diaSetInt(diaVMC, VMC_SIZE, 1);
        else if (size == 32)
            diaSetInt(diaVMC, VMC_SIZE, 2);
        else if (size == 64)
            diaSetInt(diaVMC, VMC_SIZE, 3);
        else {
            diaSetInt(diaVMC, VMC_SIZE, 0);
            diaSetLabel(diaVMC, VMC_STATUS, _l(_STR_VMC_FILE_ERROR));
        }

        diaSetLabel(diaVMC, VMC_BUTTON_CREATE, _l(_STR_MODIFY));
        diaSetVisible(diaVMC, VMC_BUTTON_DELETE, 1);
        if (gEnableWrite) {
            diaSetEnabled(diaVMC, VMC_SIZE, 1);
            diaSetEnabled(diaVMC, VMC_BUTTON_CREATE, 1);
            diaSetEnabled(diaVMC, VMC_BUTTON_DELETE, 1);
        } else {
            diaSetEnabled(diaVMC, VMC_SIZE, 0);
            diaSetEnabled(diaVMC, VMC_BUTTON_CREATE, 0);
            diaSetEnabled(diaVMC, VMC_BUTTON_DELETE, 0);
        }
    } else {
        diaSetLabel(diaVMC, VMC_BUTTON_CREATE, _l(_STR_CREATE));
        diaSetLabel(diaVMC, VMC_STATUS, _l(_STR_VMC_FILE_NEW));

        diaSetInt(diaVMC, VMC_SIZE, 0);
        diaSetEnabled(diaVMC, VMC_SIZE, 1);
        diaSetEnabled(diaVMC, VMC_BUTTON_CREATE, 1);
        diaSetVisible(diaVMC, VMC_BUTTON_DELETE, 0);
    }

    return size;
}

static int guiGameVMCUpdater(int modified)
{
    if (vmc_refresh) {
        vmc_refresh = 0;
        return VMC_REFRESH;
    }

    if ((vmc_operation == OPERATION_CREATING) || (vmc_operation == OPERATION_ABORTING)) {
        int result = fileXioDevctl("genvmc:", 0xC0DE0003, NULL, 0, (void *)&vmc_status, sizeof(vmc_status));
        if (result == 0) {
            diaSetLabel(diaVMC, VMC_STATUS, vmc_status.VMC_msg);
            diaSetInt(diaVMC, VMC_PROGRESS, vmc_status.VMC_progress);

            if (vmc_status.VMC_error != 0)
                LOG("GUI VMCUpdater: %d\n", vmc_status.VMC_error);

            if (vmc_status.VMC_status == 0x00) {
                diaSetLabel(diaVMC, VMC_BUTTON_CREATE, _l(_STR_OK));
                vmc_operation = OPERATION_ENDING;
                return VMC_BUTTON_CREATE;
            }
        } else
            LOG("GUI Status result: %d\n", result);
    }

    return 0;
}

static int guiGameShowVMCConfig(int id, item_list_t *support, char *VMCName, int slot, int validate)
{
    int result = validate ? VMC_BUTTON_CREATE : 0;
    char vmc[32];

    if (strlen(VMCName))
        strncpy(vmc, VMCName, sizeof(vmc));
    else {
        if (validate)
            return 1; // nothing to validate if no user input

        char *startup = support->itemGetStartup(support, id);
        const char *vmcTitle = getGroupIdForTitleId(startup);
        snprintf(vmc, sizeof(vmc), "%s_%d", vmcTitle, slot);
    }

    vmc_refresh = 0;
    vmc_operation = OPERATION_CREATE;
    diaSetEnabled(diaVMC, VMC_NAME, 1);
    diaSetEnabled(diaVMC, VMC_SIZE, 1);
    diaSetInt(diaVMC, VMC_PROGRESS, 0);

    const char *VMCSizes[] = {"8 Mb", "16 Mb", "32 Mb", "64 Mb", NULL};
    diaSetEnum(diaVMC, VMC_SIZE, VMCSizes);
    int size = guiGameRefreshVMCConfig(support, vmc);
    diaSetString(diaVMC, VMC_NAME, vmc);

    do {
        if (result == VMC_BUTTON_CREATE) {
            if (vmc_operation == OPERATION_CREATE) { // User start creation of VMC
                int sizeUI;
                diaGetInt(diaVMC, VMC_SIZE, &sizeUI);
                if (sizeUI == 1)
                    sizeUI = 16;
                else if (sizeUI == 2)
                    sizeUI = 32;
                else if (sizeUI == 3)
                    sizeUI = 64;
                else
                    sizeUI = 8;

                if (sizeUI != size) {
                    support->itemCheckVMC(support, vmc, sizeUI);

                    diaSetEnabled(diaVMC, VMC_NAME, 0);
                    diaSetEnabled(diaVMC, VMC_SIZE, 0);
                    diaSetLabel(diaVMC, VMC_BUTTON_CREATE, _l(_STR_ABORT));
                    vmc_operation = OPERATION_CREATING;
                } else
                    break;
            } else if (vmc_operation == OPERATION_ENDING) {
                if (validate)
                    break; // directly close VMC config dialog

                vmc_operation = OPERATION_END;
            } else if (vmc_operation == OPERATION_END) { // User closed creation dialog of VMC
                break;
            } else if (vmc_operation == OPERATION_CREATING) { // User canceled creation of VMC
                fileXioDevctl("genvmc:", 0xC0DE0002, NULL, 0, NULL, 0);
                vmc_operation = OPERATION_ABORTING;
            }
        } else if (result == VMC_BUTTON_DELETE) {
            if (guiMsgBox(_l(_STR_DELETE_WARNING), 1, diaVMC)) {
                support->itemCheckVMC(support, vmc, -1);
                diaSetString(diaVMC, VMC_NAME, "");
                break;
            }
        } else if (result == VMC_REFRESH) { // User changed the VMC name
            diaGetString(diaVMC, VMC_NAME, vmc, sizeof(vmc));
            size = guiGameRefreshVMCConfig(support, vmc);
        }

        result = diaExecuteDialog(diaVMC, result, 1, &guiGameVMCUpdater);
        if ((result == 0) && (vmc_operation == OPERATION_CREATE))
            break;

    } while (1);

    return result;
}

void guiGameShowVMCMenu(int id, item_list_t *support)
{
    int result = -1;

    // show dialog
    do {
        diaSetLabel(diaVMCConfig, COMPAT_VMC1_DEFINE, vmc1);
        diaSetLabel(diaVMCConfig, COMPAT_VMC2_DEFINE, vmc2);

        if (strlen(vmc1))
            diaSetLabel(diaVMCConfig, COMPAT_VMC1_ACTION, _l(_STR_RESET));
        else
            diaSetLabel(diaVMCConfig, COMPAT_VMC1_ACTION, _l(_STR_USE_GENERIC));
        if (strlen(vmc2))
            diaSetLabel(diaVMCConfig, COMPAT_VMC2_ACTION, _l(_STR_RESET));
        else
            diaSetLabel(diaVMCConfig, COMPAT_VMC2_ACTION, _l(_STR_USE_GENERIC));

        result = diaExecuteDialog(diaVMCConfig, result, 1, NULL);
        if (result == COMPAT_VMC1_DEFINE) {
            if (menuCheckParentalLock() == 0) {
                if (guiGameShowVMCConfig(id, support, vmc1, 0, 0))
                    diaGetString(diaVMC, VMC_NAME, vmc1, sizeof(vmc1));
            }
        } else if (result == COMPAT_VMC2_DEFINE) {
            if (menuCheckParentalLock() == 0) {
                if (guiGameShowVMCConfig(id, support, vmc2, 1, 0))
                    diaGetString(diaVMC, VMC_NAME, vmc2, sizeof(vmc2));
            }
        } else if (result == COMPAT_VMC1_ACTION) {
            if (menuCheckParentalLock() == 0) {
                if (strlen(vmc1))
                    vmc1[0] = '\0';
                else
                    snprintf(vmc1, sizeof(vmc1), "generic_%d", 0);
            }
        } else if (result == COMPAT_VMC2_ACTION) {
            if (menuCheckParentalLock() == 0) {
                if (strlen(vmc2))
                    vmc2[0] = '\0';
                else
                    snprintf(vmc2, sizeof(vmc2), "generic_%d", 1);
            }
        }
    } while (result >= COMPAT_NOEXIT);

    guiGameShowVMCConfig(id, support, vmc1, 0, 1);
    guiGameShowVMCConfig(id, support, vmc2, 1, 1);
}

// GSM
static void guiGameSetGSMSettingsState(void)
{
    int previousSource = gGSMSource;

    diaGetInt(diaGSConfig, GSMCFG_GSMSOURCE, &gGSMSource);

    // update GUI to display per-game or global settings if changed
    if (previousSource != gGSMSource && gGSMSource == SETTINGS_GLOBAL) {
        config_set_t *configSet = gameMenuLoadConfig(diaGSConfig);
        configRemoveKey(configSet, CONFIG_ITEM_GSMSOURCE);
        guiGameLoadGSMConfig(configSet, configGetByType(CONFIG_GAME));
    } else if (previousSource != gGSMSource && gGSMSource == SETTINGS_PERGAME) {
        config_set_t *configSet = gameMenuLoadConfig(diaGSConfig);
        configSetInt(configSet, CONFIG_ITEM_GSMSOURCE, gGSMSource);
        guiGameLoadGSMConfig(configSet, configGetByType(CONFIG_GAME));
    }

    diaGetInt(diaGSConfig, GSMCFG_ENABLEGSM, &EnableGSM);
    diaSetEnabled(diaGSConfig, GSMCFG_GSMVMODE, EnableGSM);
    diaSetEnabled(diaGSConfig, GSMCFG_GSMXOFFSET, EnableGSM);
    diaSetEnabled(diaGSConfig, GSMCFG_GSMYOFFSET, EnableGSM);
    diaSetEnabled(diaGSConfig, GSMCFG_GSMFIELDFIX, EnableGSM);
}

static int guiGameGSMUpdater(int modified)
{
    if (modified) {
        guiGameSetGSMSettingsState();
    }

    return 0;
}

void guiGameShowGSConfig(void)
{
    // configure the enumerations
    const char *settingsSource[] = {_l(_STR_GLOBAL_SETTINGS), _l(_STR_PERGAME_SETTINGS), NULL};
    // clang-format off
    const char *gsmvmodeNames[] = {
        "NTSC",
        "NTSC Non Interlaced",
        "PAL",
        "PAL Non Interlaced",
        "PAL @60Hz",
        "PAL @60Hz Non Interlaced",
        "PS1 NTSC (HDTV 480p @60Hz)",
        "PS1 PAL (HDTV 576p @50Hz)",
        "HDTV 480p @60Hz",
        "HDTV 576p @50Hz",
        "HDTV 720p @60Hz",
        "HDTV 1080i @60Hz",
        "HDTV 1080i @60Hz Non Interlaced",
        "VGA 640x480p @60Hz",
        "VGA 640x480p @72Hz",
        "VGA 640x480p @75Hz",
        "VGA 640x480p @85Hz",
        "VGA 640x960i @60Hz",
        "VGA 800x600p @56Hz",
        "VGA 800x600p @60Hz",
        "VGA 800x600p @72Hz",
        "VGA 800x600p @75Hz",
        "VGA 800x600p @85Hz",
        "VGA 1024x768p @60Hz",
        "VGA 1024x768p @70Hz",
        "VGA 1024x768p @75Hz",
        "VGA 1024x768p @85Hz",
        "VGA 1280x1024p @60Hz",
        "VGA 1280x1024p @75Hz",
        NULL};
    // clang-format on

    diaSetEnum(diaGSConfig, GSMCFG_GSMSOURCE, settingsSource);
    diaSetEnum(diaGSConfig, GSMCFG_GSMVMODE, gsmvmodeNames);

    diaSetEnabled(diaGSConfig, GSMCFG_GSMVMODE, EnableGSM);
    diaSetEnabled(diaGSConfig, GSMCFG_GSMXOFFSET, EnableGSM);
    diaSetEnabled(diaGSConfig, GSMCFG_GSMYOFFSET, EnableGSM);
    diaSetEnabled(diaGSConfig, GSMCFG_GSMFIELDFIX, EnableGSM);

    diaExecuteDialog(diaGSConfig, -1, 1, &guiGameGSMUpdater);
}

// CHEATS
static void guiGameSetCheatSettingsState(void)
{
    int previousSource = gCheatSource;

    diaGetInt(diaCheatConfig, CHTCFG_CHEATSOURCE, &gCheatSource);

    // update GUI to display per-game or global settings if changed
    if (previousSource != gCheatSource && gCheatSource == SETTINGS_GLOBAL) {
        config_set_t *configSet = gameMenuLoadConfig(diaCheatConfig);
        configRemoveKey(configSet, CONFIG_ITEM_CHEATSSOURCE);
        guiGameLoadCheatsConfig(configSet, configGetByType(CONFIG_GAME));
    } else if (previousSource != gCheatSource && gCheatSource == SETTINGS_PERGAME) {
        config_set_t *configSet = gameMenuLoadConfig(diaCheatConfig);
        configSetInt(configSet, CONFIG_ITEM_CHEATSSOURCE, gCheatSource);
        guiGameLoadCheatsConfig(configSet, configGetByType(CONFIG_GAME));
    }

    diaGetInt(diaCheatConfig, CHTCFG_ENABLECHEAT, &EnableCheat);
    diaGetInt(diaCheatConfig, CHTCFG_CHEATMODE, &CheatMode);
    diaSetEnabled(diaCheatConfig, CHTCFG_CHEATMODE, EnableCheat);
}

static int guiGameCheatUpdater(int modified)
{
    if (modified) {
        guiGameSetCheatSettingsState();
    }

    return 0;
}

void guiGameShowCheatConfig(void)
{
    // configure the enumerations
    const char *settingsSource[] = {_l(_STR_GLOBAL_SETTINGS), _l(_STR_PERGAME_SETTINGS), NULL};
    const char *cheatmodeNames[] = {_l(_STR_CHEATMODEAUTO), _l(_STR_CHEATMODESELECT), NULL};

    diaSetEnum(diaCheatConfig, CHTCFG_CHEATSOURCE, settingsSource);
    diaSetEnum(diaCheatConfig, CHTCFG_CHEATMODE, cheatmodeNames);
    diaSetEnabled(diaCheatConfig, CHTCFG_CHEATMODE, EnableCheat);

    diaExecuteDialog(diaCheatConfig, -1, 1, &guiGameCheatUpdater);
}

// PADEMU
#ifdef PADEMU
// from https://www.bluetooth.com/specifications/assigned-numbers/host-controller-interface
static char *bt_ver_str[] = {
    "1.0b",
    "1.1",
    "1.2",
    "2.0 + EDR",
    "2.1 + EDR",
    "3.0 + HS",
    "4.0",
    "4.1",
    "4.2",
    "5.0",
};

static const char *PadEmuPorts_enums[][5] = {
    {"1P", "2P", NULL, NULL, NULL},
    {"1A", "1B", "1C", "1D", NULL},
    {"2A", "2B", "2C", "2D", NULL},
};

#define BtnBit_Off 16
// This enum, along with lookup tables, can be re-used anywhere an enum containing all PS2 buttons is necessary.
static const char *button_names_enum[] = {
    "Off",
    "Up", "Down", "Left", "Right",
    "L1", "R1", "L2", "R2",
    "Cross", "Circle", "Square", "Triangle",
    "L3", "R3", "Start", "Select",
    NULL};

static const char button_enum_to_bit_number[] = {
    BtnBit_Off, // Off
    DS2BtnBit_Up,
    DS2BtnBit_Down,
    DS2BtnBit_Left,
    DS2BtnBit_Right,
    DS2BtnBit_L1,
    DS2BtnBit_R1,
    DS2BtnBit_L2,
    DS2BtnBit_R2,
    DS2BtnBit_Cross,
    DS2BtnBit_Circle,
    DS2BtnBit_Square,
    DS2BtnBit_Triangle,
    DS2BtnBit_L3,
    DS2BtnBit_R3,
    DS2BtnBit_Start,
    DS2BtnBit_Select,
};

static const char bit_number_to_button_enum[] = {
    [BtnBit_Off] = 0, // Off
    [DS2BtnBit_Up] = 1,
    [DS2BtnBit_Down] = 2,
    [DS2BtnBit_Left] = 3,
    [DS2BtnBit_Right] = 4,
    [DS2BtnBit_L1] = 5,
    [DS2BtnBit_R1] = 6,
    [DS2BtnBit_L2] = 7,
    [DS2BtnBit_R2] = 8,
    [DS2BtnBit_Cross] = 9,
    [DS2BtnBit_Circle] = 10,
    [DS2BtnBit_Square] = 11,
    [DS2BtnBit_Triangle] = 12,
    [DS2BtnBit_L3] = 13,
    [DS2BtnBit_R3] = 14,
    [DS2BtnBit_Start] = 15,
    [DS2BtnBit_Select] = 16,
};


static u8 ds3_mac[6];
static u8 dg_mac[6];
static char ds3_str[18];
static char dg_str[18];
static char vid_str[5];
static char pid_str[5];
static char rev_str[5];
static char hci_str[26];
static char lmp_str[26];
static char man_str[5];
static int ds3macset = 0;
static int dgmacset = 0;
static int dg_discon = 0;
static int ver_set = 0, feat_set = 0;

static int forceGlobalPadEmu;
static int forceGlobalPadMacro;

static char *bdaddr_to_str(u8 *bdaddr, char *addstr)
{
    snprintf(addstr, 18, "%02X:%02X:%02X:%02X:%02X:%02X", bdaddr[0], bdaddr[1], bdaddr[2], bdaddr[3], bdaddr[4], bdaddr[5]);

    return addstr;
}

static char *hex_to_str(char *str, u16 hex)
{
    sprintf(str, "%04X", hex);

    return str;
}

static char *ver_to_str(char *str, u8 ma, u16 mi)
{
    if (ma > 9)
        ma = 0;

    sprintf(str, "%X.%04X    BT %s", ma, mi, bt_ver_str[ma]);

    return str;
}

static int padEmuMtapPrev; // multitap state when the dialog opened / at the last update: the default slots are set only when it is switched on

// Bits of PadEmuSettings that the main dialog owns: mode (byte 0), emulation of the 4 slots (bits 8-11), vibration of the 4 slots (bits 16-19),
// multitap on/off and multitap port (bits 24-25). The fake DS3 workaround (bit 26) belongs to the Bluetooth Pads dialog.
#define PADEMU_DIALOG_MASK 0x030F0FFF

// What each port / multitap slot does: one choice per row (emulation bit + vibration bit of the slot)
#define SLOT_OFF        0
#define SLOT_ON         1
#define SLOT_ON_VIBRATE 2
// "Off" means the real controller for a physical port (Port 1/2, or slot A of the multitap = the controller plugged in the port itself);
// the other multitap slots have no real controller behind them, so Off means empty
static const char *padEmuSlotModesReal[] = {"Real controller", "Emulated", "Emulated + Vibration", NULL};
static const char *padEmuSlotModesEmpty[] = {"Empty", "Emulated", "Emulated + Vibration", NULL};
static char padEmuSlotNames[4][12];
static char padEmuOtherName[12];

static int guiGamePadEmuUpdater(int modified)
{
    int PadEmuMode = 0, PadEmuMtap = 0, PadEmuMtapPort = 1, i, slots, shown, bt;
    int val[4] = {SLOT_OFF, SLOT_OFF, SLOT_OFF, SLOT_OFF};
    int previousSource = gPadEmuSource;

    diaGetInt(diaPadEmuConfig, PADCFG_PADEMU_SOURCE, &gPadEmuSource);

    // update GUI to display per-game or global settings if changed
    if (previousSource != gPadEmuSource && gPadEmuSource == SETTINGS_GLOBAL) {
        config_set_t *configSet = gameMenuLoadConfig(diaPadEmuConfig);
        configRemoveKey(configSet, CONFIG_ITEM_PADEMUSOURCE);
        guiGameLoadPadEmuConfig(configSet, configGetByType(CONFIG_GAME));
    } else if (previousSource != gPadEmuSource && gPadEmuSource == SETTINGS_PERGAME) {
        config_set_t *configSet = gameMenuLoadConfig(diaPadEmuConfig);
        configSetInt(configSet, CONFIG_ITEM_PADEMUSOURCE, gPadEmuSource);
        guiGameLoadPadEmuConfig(configSet, configGetByType(CONFIG_GAME));
    }

    diaGetInt(diaPadEmuConfig, PADCFG_PADEMU_ENABLE, &EnablePadEmu);
    diaGetInt(diaPadEmuConfig, PADCFG_PADEMU_MODE, &PadEmuMode);
    diaGetInt(diaPadEmuConfig, PADCFG_PADEMU_MTAP, &PadEmuMtap);
    diaGetInt(diaPadEmuConfig, PADCFG_PADEMU_MTAP_PORT, &PadEmuMtapPort);
    for (i = 0; i < 4; i++)
        diaGetInt(diaPadEmuConfig, PADCFG_SLOT_EMU0 + i, &val[i]);

    slots = PadEmuMtap ? 4 : 2; // rows of the slot table in use: 1P/2P, or the four slots of the multitap

    if (modified) {
        if (PadEmuMtap && !padEmuMtapPrev) {
            // multitap just switched on: slots B, C and D start emulated; after that each slot keeps its own choice
            for (i = 1; i < 4; i++) {
                if (val[i] == SLOT_OFF)
                    val[i] = SLOT_ON;
            }
        }
        if (!PadEmuMtap) {
            // without the multitap there are only the two ports
            val[2] = val[3] = SLOT_OFF;
        }
        for (i = 0; i < 4; i++)
            diaSetInt(diaPadEmuConfig, PADCFG_SLOT_EMU0 + i, val[i]);
    }
    padEmuMtapPrev = PadEmuMtap;

    bt = (PadEmuMode == 1) & EnablePadEmu;

    diaSetEnabled(diaPadEmuConfig, PADCFG_PADEMU_MTAP, EnablePadEmu);
    diaSetEnabled(diaPadEmuConfig, PADCFG_PADEMU_MTAP_PORT, PadEmuMtap & EnablePadEmu);
    diaSetEnabled(diaPadEmuConfig, PADCFG_PADEMU_MODE, EnablePadEmu);

    // ports to emulate: names follow the multitap (Port 1 / 2, or 1A to 1D / 2A to 2D), rows without a slot are hidden
    for (i = 0; i < 4; i++) {
        shown = i < slots;

        if (shown) {
            if (PadEmuMtap)
                snprintf(padEmuSlotNames[i], sizeof(padEmuSlotNames[i]), "Port %s", PadEmuPorts_enums[PadEmuMtapPort][i]);
            else
                snprintf(padEmuSlotNames[i], sizeof(padEmuSlotNames[i]), "Port %d", i + 1);
            diaSetLabel(diaPadEmuConfig, PADCFG_SLOT_LABEL0 + i, padEmuSlotNames[i]);
        }

        diaSetEnum(diaPadEmuConfig, PADCFG_SLOT_EMU0 + i, (i == 0 || !PadEmuMtap) ? padEmuSlotModesReal : padEmuSlotModesEmpty);
        diaSetVisible(diaPadEmuConfig, PADCFG_SLOT_LABEL0 + i, shown);
        diaSetVisible(diaPadEmuConfig, PADCFG_SLOT_EMU0 + i, shown);
        diaSetEnabled(diaPadEmuConfig, PADCFG_SLOT_EMU0 + i, EnablePadEmu);
    }

    // the physical port that does not carry the emulated multitap
    snprintf(padEmuOtherName, sizeof(padEmuOtherName), "Port %d", 3 - PadEmuMtapPort);
    diaSetLabel(diaPadEmuConfig, PADCFG_PORT_OTHER_LABEL, padEmuOtherName);
    diaSetVisible(diaPadEmuConfig, PADCFG_PORT_OTHER_LABEL, PadEmuMtap);
    diaSetVisible(diaPadEmuConfig, PADCFG_PORT_OTHER_VALUE, PadEmuMtap);

    // pad behaviour and wireless pads only exist in Bluetooth mode
    diaSetVisible(diaPadEmuConfig, PADCFG_SETUP, bt);
    diaSetVisible(diaPadEmuConfig, PADCFG_SETUP_STR, bt);
    diaSetVisible(diaPadEmuConfig, PADCFG_BT_OPEN, bt);
    diaSetVisible(diaPadEmuConfig, PADCFG_BT_OPEN_STR, bt);

    // PadEmuSettings is rebuilt from the dialog; bits the dialog does not own are left alone
    PadEmuSettings &= ~PADEMU_DIALOG_MASK;
    PadEmuSettings |= PadEmuMode & 0xFF;
    for (i = 0; i < slots; i++)
        PadEmuSettings |= (((val[i] >= SLOT_ON) & 1) << (8 + i)) | (((val[i] == SLOT_ON_VIBRATE) & 1) << (16 + i));
    PadEmuSettings |= ((PadEmuMtap & 1) << 24) | (((PadEmuMtapPort - 1) & 1) << 25);

    return 0;
}

/*
 * Bluetooth Pads dialog: the adapter, the pads connected right now (with a Disconnect button each),
 * Search / Pair new pad, DualShock 3 pairing through the USB cable and the adapter information.
 */
static char btPadNames[DS34BT_PADS_MAX][40];
static int btPadsTimer;

static void guiGameBtPadsRefresh(void)
{
    static const char *types[] = {"", "DualShock 3", "DualShock 4", "DualSense"};
    int i, any = 0;

    ds34bt_update_info(); // one RPC for all the ports

    for (i = 0; i < DS34BT_PADS_MAX; i++) {
        const ds34bt_info_t *info = ds34bt_cached_info(i);
        int on = info->connected;

        if (on) {
            const char *type = types[info->type <= DS34BT_TYPE_DS5 ? info->type : 0];

            if (info->percent <= 100)
                snprintf(btPadNames[i], sizeof(btPadNames[i]), "Pad %d: %s %d%%", i + 1, type, info->percent);
            else
                snprintf(btPadNames[i], sizeof(btPadNames[i]), "Pad %d: %s", i + 1, type);
            diaSetLabel(diaPadEmuBtPads, PADCFG_BTP_LABEL0 + i, btPadNames[i]);
            any = 1;
        }

        diaSetVisible(diaPadEmuBtPads, PADCFG_BTP_LABEL0 + i, on);
        diaSetVisible(diaPadEmuBtPads, PADCFG_BTP_DISC0 + i, on);
    }

    diaSetVisible(diaPadEmuBtPads, PADCFG_BTP_NONE, !any);
}

static int guiGamePadEmuBtPadsUpdater(int modified)
{
    int workaround = 0;

    // the workaround lives in this dialog but is stored with the rest of the settings
    diaGetInt(diaPadEmuBtPads, PADCFG_PADEMU_WORKAROUND, &workaround);
    PadEmuSettings = (PadEmuSettings & ~(1 << 26)) | ((workaround & 1) << 26);

    if ((btPadsTimer++ % 30) == 0) // about twice per second
        guiGameBtPadsRefresh();

    if (ds34bt_get_status(0) & DS34BT_STATE_USB_CONFIGURED) {
        if (dg_discon) {
            dgmacset = 0;
            dg_discon = 0;
        }
        if (!dgmacset) {
            if (ds34bt_get_bdaddr(dg_mac)) {
                dgmacset = 1;
                diaSetLabel(diaPadEmuBtPads, PADCFG_USBDG_MAC, bdaddr_to_str(dg_mac, dg_str));
            } else {
                dgmacset = 0;
            }
        }
    } else {
        dg_discon = 1;
    }

    if (!dgmacset)
        diaSetLabel(diaPadEmuBtPads, PADCFG_USBDG_MAC, _l(_STR_NOT_CONNECTED));

    if (ds34usb_get_status(0) & DS34USB_STATE_RUNNING) {
        if (!ds3macset) {
            if (ds34usb_get_bdaddr(0, ds3_mac)) {
                ds3macset = 1;
                diaSetLabel(diaPadEmuBtPads, PADCFG_PAD_MAC, bdaddr_to_str(ds3_mac, ds3_str));
            } else {
                ds3macset = 0;
            }
        }
    } else {
        diaSetLabel(diaPadEmuBtPads, PADCFG_PAD_MAC, _l(_STR_NOT_CONNECTED));
        ds3macset = 0;
    }

    // the DS3 pairing rows only make sense with a DualShock 3 plugged in
    diaSetVisible(diaPadEmuBtPads, PADCFG_PAD_MAC_STR, ds3macset);
    diaSetVisible(diaPadEmuBtPads, PADCFG_PAD_MAC, ds3macset);
    diaSetVisible(diaPadEmuBtPads, PADCFG_PAIR_STR, ds3macset);
    diaSetVisible(diaPadEmuBtPads, PADCFG_PAIR, ds3macset);

    return 0;
}

static int guiGamePadEmuInfoUpdater(int modified)
{
    hci_information_t info;
    u8 feat[8];
    int i, j;
    u8 data;
    int supported;

    if (ds34bt_get_status(0) & DS34BT_STATE_USB_CONFIGURED) {
        if (!ver_set) {
            if (ds34bt_get_version(&info)) {
                ver_set = 1;
                diaSetLabel(diaPadEmuInfo, PADCFG_VID, hex_to_str(vid_str, info.vid));
                diaSetLabel(diaPadEmuInfo, PADCFG_PID, hex_to_str(pid_str, info.pid));
                diaSetLabel(diaPadEmuInfo, PADCFG_REV, hex_to_str(rev_str, info.rev));
                diaSetLabel(diaPadEmuInfo, PADCFG_HCIVER, ver_to_str(hci_str, info.hci_ver, info.hci_rev));
                diaSetLabel(diaPadEmuInfo, PADCFG_LMPVER, ver_to_str(lmp_str, info.lmp_ver, info.lmp_subver));
                diaSetLabel(diaPadEmuInfo, PADCFG_MANID, hex_to_str(man_str, info.mf_name));
            } else {
                ver_set = 0;
            }
        }

        if (!feat_set) {
            if (ds34bt_get_features(feat)) {
                feat_set = 1;
                supported = 0;
                for (i = 0, j = 0; i < 64; i++) {
                    data = (feat[j] >> (i - j * 8)) & 1;
                    diaSetLabel(diaPadEmuInfo, PADCFG_FEAT_START + i, _l(_STR_NO - data));
                    j = (i + 1) / 8;
                    if (i == 25 || i == 26 || i == 39) {
                        if (data)
                            supported++;
                    }
                }
                if (supported == 3)
                    diaSetLabel(diaPadEmuInfo, PADCFG_BT_SUPPORTED, _l(_STR_BT_SUPPORTED));
                else
                    diaSetLabel(diaPadEmuInfo, PADCFG_BT_SUPPORTED, _l(_STR_BT_NOTSUPPORTED));
            } else {
                feat_set = 0;
            }
        }
    } else {
        ver_set = 0;
        feat_set = 0;
    }

    return 0;
}

/*
 * Wireless pairing (DS4 / DualSense in pairing mode, Secure Simple Pairing).
 * The link key of each pad paired this way is kept in conf_opl.cfg and handed to
 * the menu driver (ds34bt) and to the in-game driver (bt_pademu).
 */
#define BTS_RESULT_PAIRED 0x7F000001 // returned by the search dialog updater

static bt_link_key_t btKeys[BTKEYS_MAX];
static char btsLabels[DS34BT_SEARCH_MAX][DS34BT_NAME_LEN + 24];
static char btsNames[DS34BT_SEARCH_MAX][DS34BT_NAME_LEN];
static char btsStatus[128];
static char btsDbg[4][72];
static int btsCount, btsShown, btsPairIdx, btsPairStart, btsTimer;

/*
 * Controller Setup (button remap, PS button, touchpad): stored as hex in the OPL config, handed to bt_pademu
 * at game launch (guiGameSetupModuleArg, in-memory copy because OPL frees its config before launching).
 */
static padsetup_t gPadSetup;
static int gPadSetupLoaded = 0;

static void guiGameSetupLoad(void)
{
    char str[PADSETUP_HEX_LEN + 1];
    config_set_t *cfg = configGetByType(CONFIG_OPL);

    str[0] = '\0';
    if (cfg)
        configGetStrCopy(cfg, CONFIG_ITEM_PADEMU_SETUP, str, sizeof(str));
    padsetup_from_hex(str, &gPadSetup); // default setup if missing or invalid
    gPadSetupLoaded = 1;
}

static void guiGameSetupSave(void)
{
    char str[PADSETUP_HEX_LEN + 1];

    padsetup_to_hex(&gPadSetup, str);
    configSetStr(configGetByType(CONFIG_OPL), CONFIG_ITEM_PADEMU_SETUP, str);
    saveConfig(CONFIG_OPL, 0);
}

void guiGameSetupModuleArg(unsigned char *out)
{
    if (!gPadSetupLoaded)
        padsetup_default(&gPadSetup);
    memcpy(out, &gPadSetup, PADSETUP_ARG_LEN);
}

static int btKeysCount = 0; // copy of the saved keys: OPL frees its config (configEnd) before launching a game

static int guiGameBtKeysLoad(void)
{
    char str[BTKEYS_HEX_LEN + 1];

    configGetStrCopy(configGetByType(CONFIG_OPL), CONFIG_ITEM_PADEMU_BTKEYS, str, sizeof(str));
    btKeysCount = btkeys_parse(str, btKeys, BTKEYS_MAX);
    return btKeysCount;
}

void guiGameBtKeysPushToDriver(void)
{
    int n = guiGameBtKeysLoad(), i;

    guiGameSetupLoad(); // Controller Setup lives in the same config: keep an in-memory copy for the game launch

    ds34bt_set_key(NULL, NULL);
    for (i = 0; i < n; i++)
        ds34bt_set_key(btKeys[i].bdaddr, btKeys[i].key);
}

static volatile int btKeysPushPending = 0;

// Called from any thread: the push itself happens in the main thread (readPads), so RPC calls to ds34bt never overlap.
void guiGameBtKeysRequestPush(void)
{
    btKeysPushPending = 1;
}

void guiGameBtKeysPushIfPending(void)
{
    if (btKeysPushPending) {
        btKeysPushPending = 0;
        guiGameBtKeysPushToDriver();
    }
}

// Uses the in-memory copy only: at game launch the OPL config has already been freed.
void guiGameBtKeysModuleArg(char *out)
{
    out[0] = '\0';
    if (btKeysCount > 0) {
        strcpy(out, BTKEYS_ARG_TAG);
        btkeys_format(btKeys, btKeysCount, out + strlen(BTKEYS_ARG_TAG));
    }
}

static void guiGameBtKeysSave(const u8 *bdaddr, const u8 *key)
{
    char str[BTKEYS_HEX_LEN + 1];
    int n = guiGameBtKeysLoad();

    n = btkeys_store(btKeys, n, bdaddr, key);
    btKeysCount = n;
    btkeys_format(btKeys, n, str);
    configSetStr(configGetByType(CONFIG_OPL), CONFIG_ITEM_PADEMU_BTKEYS, str);
    saveConfig(CONFIG_OPL, 0);
}

static void guiGameBtKeysSaveAll(void)
{
    char str[BTKEYS_HEX_LEN + 1];

    btkeys_format(btKeys, btKeysCount, str);
    configSetStr(configGetByType(CONFIG_OPL), CONFIG_ITEM_PADEMU_BTKEYS, str);
    saveConfig(CONFIG_OPL, 0);
}

/*
 * Lightbar colour of a controller (Controller Setup). The colour belongs to the pad: it is stored in the saved pairing
 * entry of that pad (btKeys[].led, same config value as the link key), so it follows the pad whatever slot it connects
 * to, in the menu and inside the game (bt_pademu gets the same table). A pad without a saved pairing (cable paired)
 * keeps using the colour of its slot (padsetup led[]), and so does a pad whose colour was never chosen.
 */
static signed char btSlotKey[DS34BT_PADS_MAX] = {-1, -1, -1, -1}; // btKeys index of the pad running in each slot, -1 = none/unknown

static void guiGameBtResolveSlot(int slot)
{
    u8 addr[6];

    if (slot < 0 || slot >= DS34BT_PADS_MAX)
        return;

    btSlotKey[slot] = -1;
    if (btKeysCount > 0 && ds34bt_get_pad_addr(slot, addr))
        btSlotKey[slot] = (signed char)btkeys_find(btKeys, btKeysCount, addr);
}

static int guiGameBtLedIdx(int slot)
{
    int k;

    if (!gPadSetupLoaded)
        guiGameSetupLoad();

    k = (slot >= 0 && slot < DS34BT_PADS_MAX) ? btSlotKey[slot] : -1;
    if (k >= 0 && k < btKeysCount && btKeys[k].led && btKeys[k].led < PADSETUP_LED_COUNT)
        return btKeys[k].led;

    return padsetup_get_led(&gPadSetup, slot);
}

void guiGameBtLedApply(int slot)
{
    u8 led[4];
    int idx = guiGameBtLedIdx(slot);

    padsetup_led_rgb_of(&gPadSetup, slot, idx, led);
    led[3] = idx ? 0x02 : 0x00; // bit1: colour chosen by the user (kept by the driver), 0 = default pattern of the slot
    ds34bt_set_led(slot, led);
}

// Called when a pad connects to the menu driver: finds out which saved pad it is, then sends its colour if one was chosen.
void guiGameBtLedOnConnect(int slot)
{
    guiGameBtResolveSlot(slot);

    if (guiGameBtLedIdx(slot))
        guiGameBtLedApply(slot);
}

void guiGameBtLedOnDisconnect(int slot)
{
    if (slot >= 0 && slot < DS34BT_PADS_MAX)
        btSlotKey[slot] = -1;
}

// Colour of the pad in the slot (rgb[3]); returns 0 when it uses its default colour
int guiGameBtLedGet(int slot, u8 *rgb)
{
    int idx = guiGameBtLedIdx(slot);

    if (!idx)
        return 0;

    padsetup_led_rgb_of(&gPadSetup, slot, idx, rgb);
    return 1;
}

// Controller Setup opens: the row of each slot shows the colour saved for the pad that is running there.
static void guiGameBtLedSyncRows(void)
{
    int i, k;

    ds34bt_update_info();
    for (i = 0; i < PADSETUP_LED_SLOTS; i++) {
        if (ds34bt_cached_info(i)->connected && btSlotKey[i] < 0)
            guiGameBtResolveSlot(i);

        k = btSlotKey[i];
        if (k >= 0 && k < btKeysCount && btKeys[k].led && btKeys[k].led < PADSETUP_LED_COUNT)
            padsetup_set_led(&gPadSetup, i, btKeys[k].led);
    }
}

static void guiGameBtSearchShowDevices(int show)
{
    int i;

    for (i = 0; i < DS34BT_SEARCH_MAX; i++)
        diaSetVisible(diaPadEmuBtSearch, PADCFG_BTS_DEV0 + i, show && i < btsCount);
}

// Adapter / driver state for troubleshooting (layout of d[]: DBG_* in modules/ds34bt/iop/ds34bt.c)
static int guiGameBtDebugUpdate(u8 *d)
{
    char mode[4];

    if (!ds34bt_get_debug(d)) {
        snprintf(btsDbg[0], sizeof(btsDbg[0]), "debug: ds34bt driver does not answer");
        btsDbg[1][0] = btsDbg[2][0] = btsDbg[3][0] = '\0';
        return 0;
    }

    if (d[30])
        snprintf(mode, sizeof(mode), "%02X", d[30] - 1);
    else
        snprintf(mode, sizeof(mode), "-");

    // usb flags (02 = adapter ready) | init steps done of 7 | adapter resets | usb errors | hci events | inquiry mode answer
    snprintf(btsDbg[0], sizeof(btsDbg[0]), "USB:%02X stage:%d/7 rst:%d uerr:%d ev:%d imode:%s", d[0], d[1], d[6], d[31], d[28] | (d[29] << 8), mode);
    // last command answer (opcode=status) | last command error | last event | events the driver ignores
    snprintf(btsDbg[1], sizeof(btsDbg[1]), "cmd:%02X%02X=%02X err:%02X(%02X%02X) evt:%02X unk:%d/%02X", d[2], d[3], d[4], d[7], d[8], d[9], d[5], d[17], d[18]);
    // inquiry results (plain / with RSSI) | inquiry ends | devices listed | skipped by class | class of the last device seen
    snprintf(btsDbg[2], sizeof(btsDbg[2]), "inq:%d/%d done:%d list:%d skip:%d cod:%02X%02X%02X", d[12], d[13], d[14], d[15], d[16], d[21], d[20], d[19]);
    snprintf(btsDbg[3], sizeof(btsDbg[3]), "last dev: %02X:%02X:%02X:%02X:%02X:%02X  search:%d pair:%d", d[27], d[26], d[25], d[24], d[23], d[22], d[10], d[11]);

    return 1;
}

// The adapter / driver lines are for troubleshooting only: they appear when something went wrong
static void guiGameBtSearchShowDebug(int show)
{
    int i;

    for (i = 0; i < 4; i++)
        diaSetVisible(diaPadEmuBtSearch, PADCFG_BTS_DBG0 + i, show);
}

static void guiGameBtSearchStart(void)
{
    u8 d[DS34BT_DEBUG_SIZE];

    btsCount = 0;
    btsShown = 0;
    guiGameBtSearchShowDevices(0);
    guiGameBtSearchShowDebug(0);

    if (ds34bt_search_start()) {
        snprintf(btsStatus, sizeof(btsStatus), "Searching...");
    } else if (guiGameBtDebugUpdate(d)) {
        snprintf(btsStatus, sizeof(btsStatus), "Bluetooth adapter not ready (init stage %d/7). See below.", d[1]);
        guiGameBtSearchShowDebug(1);
    } else {
        snprintf(btsStatus, sizeof(btsStatus), "Bluetooth adapter not ready.");
        guiGameBtSearchShowDebug(1);
    }
}

static int guiGameBtSearchUpdater(int modified)
{
    u8 bdaddr[6], key[16], btDbgRaw[DS34BT_DEBUG_SIZE];
    int state, i, count;

    if ((++btsTimer % 15) != 0) // poll the driver about 4 times per second
        return 0;

    guiGameBtDebugUpdate(btDbgRaw);

    if (btsPairIdx >= 0) {
        state = ds34bt_get_pair(bdaddr, key);
        if (state == DS34BT_PAIR_DONE) {
            return BTS_RESULT_PAIRED;
        } else if (state == DS34BT_PAIR_FAILED || state < 0 || (btsTimer - btsPairStart) > 45 * 60) {
            snprintf(btsStatus, sizeof(btsStatus), "Pairing failed. Put the pad in pairing mode and search again.");
            btsPairIdx = -1;
        }
        return 0;
    }

    state = ds34bt_get_search(0, &count, NULL, NULL);
    switch (state) {
        case DS34BT_SEARCH_INQUIRY:
            snprintf(btsStatus, sizeof(btsStatus), "Searching...");
            break;
        case DS34BT_SEARCH_NAMES:
            snprintf(btsStatus, sizeof(btsStatus), "Searching... reading names");
            break;
        case DS34BT_SEARCH_ERROR:
            snprintf(btsStatus, sizeof(btsStatus), "Search failed. Try again.");
            guiGameBtSearchShowDebug(1);
            break;
        case DS34BT_SEARCH_DONE:
            if (btsShown)
                break;
            btsShown = 1;
            btsCount = count > DS34BT_SEARCH_MAX ? DS34BT_SEARCH_MAX : count;
            for (i = 0; i < btsCount; i++) {
                ds34bt_get_search(i, NULL, bdaddr, btsNames[i]);
                if (btsNames[i][0] == '\0')
                    snprintf(btsNames[i], DS34BT_NAME_LEN, "Unknown device");
                snprintf(btsLabels[i], sizeof(btsLabels[i]), "%s  (%02X:%02X:%02X:%02X:%02X:%02X)", btsNames[i],
                         bdaddr[5], bdaddr[4], bdaddr[3], bdaddr[2], bdaddr[1], bdaddr[0]);
                diaSetLabel(diaPadEmuBtSearch, PADCFG_BTS_DEV0 + i, btsLabels[i]);
            }
            guiGameBtSearchShowDevices(1);
            if (btsCount > 0)
                snprintf(btsStatus, sizeof(btsStatus), "Found %d. Select the pad to pair:", btsCount);
            else {
                snprintf(btsStatus, sizeof(btsStatus), "Nothing found. Is the light flashing fast? Search again.");
                guiGameBtSearchShowDebug(1);
            }
            break;
        default:
            break;
    }

    return 0;
}

static void guiGameShowBtSearch(void)
{
    u8 bdaddr[6], key[16];
    int result = -1, idx;

    btsPairIdx = -1;
    btsTimer = 0;
    diaSetLabel(diaPadEmuBtSearch, PADCFG_BTS_STATUS, btsStatus);
    diaSetLabel(diaPadEmuBtSearch, PADCFG_BTS_DBG0, btsDbg[0]);
    diaSetLabel(diaPadEmuBtSearch, PADCFG_BTS_DBG1, btsDbg[1]);
    diaSetLabel(diaPadEmuBtSearch, PADCFG_BTS_DBG2, btsDbg[2]);
    diaSetLabel(diaPadEmuBtSearch, PADCFG_BTS_DBG3, btsDbg[3]);
    guiGameBtSearchStart();

    while (1) {
        result = diaExecuteDialog(diaPadEmuBtSearch, result, 1, &guiGameBtSearchUpdater);

        if (result == UIID_BTN_OK || result == UIID_BTN_CANCEL) {
            if (btsPairIdx < 0)
                break; // don't leave while pairing: the key must be saved
            result = -1;
        } else if (result == PADCFG_BTS_AGAIN) {
            if (btsPairIdx < 0)
                guiGameBtSearchStart();
        } else if (result >= PADCFG_BTS_DEV0 && result <= PADCFG_BTS_DEV3) {
            idx = result - PADCFG_BTS_DEV0;
            if (btsPairIdx < 0 && idx < btsCount) {
                if (ds34bt_pair(idx)) {
                    btsPairIdx = idx;
                    btsPairStart = btsTimer;
                    snprintf(btsStatus, sizeof(btsStatus), "Pairing with %s...", btsNames[idx]);
                } else {
                    snprintf(btsStatus, sizeof(btsStatus), "Could not start pairing. Search again.");
                }
            }
        } else if (result == BTS_RESULT_PAIRED) {
            if (ds34bt_get_pair(bdaddr, key) == DS34BT_PAIR_DONE) {
                guiGameBtKeysSave(bdaddr, key);
                snprintf(btsStatus, sizeof(btsStatus), "Paired: %s. Press PS to connect.", btsNames[btsPairIdx]);
            }
            btsPairIdx = -1;
            result = -1;
        }
    }
}

// lightbar colours, shared with the Controller Setup dialog
static const char *btLedNames[] = {"Default", "Red", "Green", "Blue", "Yellow", "Cyan", "Magenta", "White", "Orange", "Off", "Custom", NULL};

/*
 * Controller Setup dialogs
 */
#define BTN16(first) first, "Select", "L3", "R3", "Start", "Up", "Right", "Down", "Left", "L2", "R2", "L1", "R1", "Triangle", "Circle", "Cross", "Square", NULL
// index = value stored in the blob: 0 = none, 1..16 = DS2 button + 1
static const char *setupTargetNames[] = {BTN16("None")};
static const char *setupTouchModes[] = {"Select/Start halves (original)", "Click = button only", "Right stick: drag", "Left stick: drag", "Right stick: mouse", "Left stick: mouse", NULL};
static const char *setupPsHold[] = {"Nothing", "Exit to OPL (IGR)", "Power off", "Screenshot (needs GSM)", "Turn off controller", NULL};
static const char *setupPsTime[] = {"1 second", "2 seconds", "3 seconds", NULL};
static const char *setupRemapEnum[19];

static void guiGameSetupBuildRemapEnum(void)
{
    int i;

    setupRemapEnum[0] = "Unchanged";
    setupRemapEnum[1] = "Disabled";
    for (i = 0; i < 16; i++)
        setupRemapEnum[2 + i] = setupTargetNames[1 + i];
    setupRemapEnum[18] = NULL;
}

/*
 * Controller Setup <-> dialog. One row per parameter of the padsetup_params[] table (include/padsetup.h):
 * the dialog item of parameter p is PADSET_TOUCH_MODE + p (checked below), gSetupLabels[p] holds its dropdown
 * labels (NULL for a plain number). A new setting only needs a row here, its dialog item and its table row.
 */
_Static_assert(PADSET_LED3 - PADSET_TOUCH_MODE == PADSETUP_P_LED3, "dialog items must follow the padsetup_params order");
#define SETUP_DIA_ID(p) (PADSET_TOUCH_MODE + (p))

static const char **const gSetupLabels[PADSETUP_P_COUNT] = {
    [PADSETUP_P_TOUCH_MODE] = setupTouchModes,
    [PADSETUP_P_TOUCH_SENS] = NULL,
    [PADSETUP_P_TOUCH_CLICK] = setupTargetNames,
    [PADSETUP_P_PS_TAP] = setupTargetNames,
    [PADSETUP_P_PS_HOLD] = setupPsHold,
    [PADSETUP_P_PS_TIME] = setupPsTime,
    [PADSETUP_P_LED0] = btLedNames,
    [PADSETUP_P_LED1] = btLedNames,
    [PADSETUP_P_LED2] = btLedNames,
    [PADSETUP_P_LED3] = btLedNames,
};

static void guiGameSetupEnumsToDialog(void)
{
    for (int p = 0; p < PADSETUP_P_COUNT; p++) {
        if (gSetupLabels[p])
            diaSetEnum(diaPadEmuSetup, SETUP_DIA_ID(p), gSetupLabels[p]);
    }
}

/*
 * Live preview: while Controller Setup is open the lightbar of each connected pad follows what is selected in its row
 * (palette entry and, for "Custom", the colour of the picker), without waiting for OK. Cancel puts the saved colours back.
 */
static int gLedPrevIdx[PADSETUP_LED_SLOTS];
static u8 gLedPrevRgb[PADSETUP_LED_SLOTS][3];

static void guiGameSetupLedPreview(int slot, int idx, const u8 *rgb)
{
    u8 led[4];

    if (gLedPrevIdx[slot] == idx && !memcmp(gLedPrevRgb[slot], rgb, 3))
        return;

    gLedPrevIdx[slot] = idx;
    memcpy(gLedPrevRgb[slot], rgb, 3);
    if (!ds34bt_cached_info(slot)->connected)
        return;

    led[0] = rgb[0];
    led[1] = rgb[1];
    led[2] = rgb[2];
    led[3] = idx ? 0x02 : 0x00; // 0 = back to the default pattern of the slot
    ds34bt_set_led(slot, led);
}

// Shows the colour picker only on the rows set to Custom, and previews every row.
static void guiGameSetupLedSync(void)
{
    for (int i = 0; i < PADSETUP_LED_SLOTS; i++) {
        int idx = 0;
        u8 rgb[3] = {0, 0, 0};

        diaGetInt(diaPadEmuSetup, PADSET_LED0 + i, &idx);
        if (idx < 0 || idx >= PADSETUP_LED_COUNT)
            idx = 0;
        diaSetVisible(diaPadEmuSetup, PADSET_LEDCOL0 + i, idx == PADSETUP_LED_CUSTOM);
        if (idx == PADSETUP_LED_CUSTOM)
            diaGetColor(diaPadEmuSetup, PADSET_LEDCOL0 + i, rgb);
        else
            memcpy(rgb, padsetup_led_rgb(idx), 3);
        guiGameSetupLedPreview(i, idx, rgb);
    }
}

static int guiGameSetupUpdater(int modified)
{
    guiGameSetupLedSync();
    return 0;
}

// Called by the colour selector on every frame while a picker is open (its value is not in the dialog until confirmed)
static void guiGameSetupColourHook(int id, const unsigned char *rgb)
{
    if (id >= PADSET_LEDCOL0 && id < PADSET_LEDCOL0 + PADSETUP_LED_SLOTS)
        guiGameSetupLedPreview(id - PADSET_LEDCOL0, PADSETUP_LED_CUSTOM, rgb);
}

static void guiGameSetupToDialog(void)
{
    for (int p = 0; p < PADSETUP_P_COUNT; p++)
        diaSetInt(diaPadEmuSetup, SETUP_DIA_ID(p), padsetup_get(&gPadSetup, p));
    for (int i = 0; i < PADSETUP_LED_SLOTS; i++)
        diaSetColor(diaPadEmuSetup, PADSET_LEDCOL0 + i, &gPadSetup.led_custom[i * 3]);
    guiGameSetupLedSync();
}

static void guiGameSetupFromDialog(void)
{
    int v;

    for (int p = 0; p < PADSETUP_P_COUNT; p++) {
        if (diaGetInt(diaPadEmuSetup, SETUP_DIA_ID(p), &v))
            padsetup_set(&gPadSetup, p, v); // out-of-range values are ignored
    }
    for (int i = 0; i < PADSETUP_LED_SLOTS; i++)
        diaGetColor(diaPadEmuSetup, PADSET_LEDCOL0 + i, &gPadSetup.led_custom[i * 3]);
}

static void guiGameShowPadRemap(void)
{
    int i, v, result = -1;

    guiGameSetupBuildRemapEnum();
    for (i = 0; i < 16; i++) {
        diaSetEnum(diaPadEmuRemap, PADREM_BASE + i, setupRemapEnum);
        diaSetInt(diaPadEmuRemap, PADREM_BASE + i, padsetup_get_remap(&gPadSetup, i));
    }

    while (1) {
        result = diaExecuteDialog(diaPadEmuRemap, result, 1, NULL);

        if (result == PADREM_RESET) {
            for (i = 0; i < 16; i++)
                diaSetInt(diaPadEmuRemap, PADREM_BASE + i, 0);
        } else if (result == UIID_BTN_OK || result == UIID_BTN_CANCEL) {
            break;
        }
    }

    for (i = 0; i < 16; i++) {
        if (diaGetInt(diaPadEmuRemap, PADREM_BASE + i, &v))
            padsetup_set_remap(&gPadSetup, i, v);
    }
}

/*
 * Public Controller Setup API (include/guigame.h). The value lives in gPadSetup, which is what bt_pademu receives at game
 * launch; guiGameSetupSave() writes it to the OPL config. A new menu layout only needs these, not the blob.
 */
int guiGameSetupGet(int param)
{
    if (!gPadSetupLoaded)
        guiGameSetupLoad();
    return padsetup_get(&gPadSetup, param);
}

int guiGameSetupSet(int param, int value)
{
    if (!gPadSetupLoaded)
        guiGameSetupLoad();
    return padsetup_set(&gPadSetup, param, value);
}

int guiGameSetupGetRemap(int button)
{
    if (!gPadSetupLoaded)
        guiGameSetupLoad();
    return padsetup_get_remap(&gPadSetup, button);
}

void guiGameSetupSetRemap(int button, int index)
{
    if (!gPadSetupLoaded)
        guiGameSetupLoad();
    padsetup_set_remap(&gPadSetup, button, index);
}

void guiGameSetupResetDefaults(void)
{
    padsetup_default(&gPadSetup);
    gPadSetupLoaded = 1;
}

// Save to the config and apply what the menu driver can use right away (lightbar colours of connected pads).
// A pad with a saved pairing keeps the colour chosen for its row in that pairing entry (so it follows the pad).
void guiGameSetupCommit(void)
{
    int i, k, changed = 0;

    guiGameSetupSave();

    ds34bt_update_info();
    for (i = 0; i < PADSETUP_LED_SLOTS; i++) {
        if (!ds34bt_cached_info(i)->connected)
            continue;

        guiGameBtResolveSlot(i);
        k = btSlotKey[i];
        if (k >= 0 && k < btKeysCount) {
            int v = padsetup_get_led(&gPadSetup, i);

            if (btKeys[k].led != v) {
                btKeys[k].led = (u8)v;
                changed = 1;
            }
        }
        guiGameBtLedApply(i); // Default = back to the pattern of the slot
    }

    if (changed)
        guiGameBtKeysSaveAll();
}

static void guiGameShowPadSetup(void)
{
    int result = -1, saved = 0;
    padsetup_t backup;

    if (!gPadSetupLoaded)
        guiGameSetupLoad();

    guiGameBtLedSyncRows();

    backup = gPadSetup; // Cancel puts this back
    guiGameSetupEnumsToDialog();
    for (int i = 0; i < PADSETUP_LED_SLOTS; i++)
        gLedPrevIdx[i] = -1; // first sync always sends
    guiGameSetupToDialog();
    diaSetColourPreview(guiGameSetupColourHook);

    while (1) {
        result = diaExecuteDialog(diaPadEmuSetup, result, 1, guiGameSetupUpdater);

        if (result == PADSET_REMAP) {
            guiGameSetupFromDialog();
            guiGameShowPadRemap();
        } else if (result == PADSET_RESET) {
            padsetup_default(&gPadSetup);
            guiGameSetupToDialog();
        } else if (result == UIID_BTN_OK) {
            saved = 1;
            break;
        } else if (result == UIID_BTN_CANCEL) {
            break;
        }
    }

    diaSetColourPreview(NULL);

    if (saved) {
        guiGameSetupFromDialog();
        guiGameSetupCommit();
    } else {
        gPadSetup = backup; // cancelled: nothing changes (the remap dialog edits gPadSetup directly)
        for (int i = 0; i < PADSETUP_LED_SLOTS; i++)
            if (ds34bt_cached_info(i)->connected)
                guiGameBtLedApply(i); // put the saved colours back
    }
}

// Bluetooth Pads: connected pads (Disconnect), Search / Pair new pad, DualShock 3 pairing, adapter information
static void guiGameShowBtPads(void)
{
    int result = -1, i;

    btPadsTimer = 0;

    while (1) {
        result = diaExecuteDialog(diaPadEmuBtPads, result, 1, &guiGamePadEmuBtPadsUpdater);

        if (result == UIID_BTN_OK || result == UIID_BTN_CANCEL || result == 0)
            break;

        if (result >= PADCFG_BTP_DISC0 && result < PADCFG_BTP_DISC0 + DS34BT_PADS_MAX) {
            ds34bt_disconnect(result - PADCFG_BTP_DISC0);
            DelayThread(300 * 1000); // let the driver close the link before the list is read again
            btPadsTimer = 0;
        }

        if (result == PADCFG_BTSEARCH) {
            guiGameShowBtSearch();
            btPadsTimer = 0;
        }

        if (result == PADCFG_PAIR) {
            if (ds3macset && dgmacset) {
                if (ds34usb_get_status(0) & DS34USB_STATE_RUNNING) {
                    if (ds34usb_set_bdaddr(0, dg_mac))
                        ds3macset = 0;
                }
            }
        }

        if (result == PADCFG_BTINFO) {
            for (i = PADCFG_FEAT_START; i < PADCFG_FEAT_END + 1; i++)
                diaSetLabel(diaPadEmuInfo, i, _l(_STR_NO));

            diaSetLabel(diaPadEmuInfo, PADCFG_VID, _l(_STR_NOT_CONNECTED));
            diaSetLabel(diaPadEmuInfo, PADCFG_PID, _l(_STR_NOT_CONNECTED));
            diaSetLabel(diaPadEmuInfo, PADCFG_REV, _l(_STR_NOT_CONNECTED));
            diaSetLabel(diaPadEmuInfo, PADCFG_HCIVER, _l(_STR_NOT_CONNECTED));
            diaSetLabel(diaPadEmuInfo, PADCFG_LMPVER, _l(_STR_NOT_CONNECTED));
            diaSetLabel(diaPadEmuInfo, PADCFG_MANID, _l(_STR_NOT_CONNECTED));
            diaSetLabel(diaPadEmuInfo, PADCFG_BT_SUPPORTED, _l(_STR_NOT_CONNECTED));
            ver_set = 0;
            feat_set = 0;
            diaExecuteDialog(diaPadEmuInfo, -1, 1, &guiGamePadEmuInfoUpdater);
        }
    }
}

void guiGameShowPadEmuConfig(int forceGlobal)
{
    const char *settingsSource[] = {_l(_STR_GLOBAL_SETTINGS), _l(_STR_PERGAME_SETTINGS), NULL};
    const char *PadEmuModes[] = {_l(_STR_DS34USB_MODE), "DS3/DS4/DS5 BT", NULL}; // fixed label: says DualSense in every language

    int PadEmuMtap, i;

    forceGlobalPadEmu = forceGlobal;
    diaSetEnabled(diaPadEmuConfig, PADCFG_PADEMU_SOURCE, !forceGlobalPadEmu);

    if (forceGlobalPadEmu)
        guiGameLoadPadEmuConfig(NULL, configGetByType(CONFIG_GAME));

    diaSetEnum(diaPadEmuConfig, PADCFG_PADEMU_SOURCE, settingsSource);
    diaSetEnum(diaPadEmuConfig, PADCFG_PADEMU_MODE, PadEmuModes);
    for (i = 0; i < 4; i++)
        diaSetEnum(diaPadEmuConfig, PADCFG_SLOT_EMU0 + i, padEmuSlotModesReal);

    PadEmuMtap = (PadEmuSettings >> 24) & 1;

    padEmuMtapPrev = PadEmuMtap;
    if (!PadEmuMtap)
        PadEmuSettings &= ~0x000C0C00; // only the two ports exist without the multitap: no emulation or vibration bits for slots C and D

    int result = -1;

    while (result != 0) {
        result = diaExecuteDialog(diaPadEmuConfig, result, 1, &guiGamePadEmuUpdater);

        if (result == PADCFG_BT_OPEN)
            guiGameShowBtPads();

        if (result == PADCFG_SETUP)
            guiGameShowPadSetup();

        if (result == UIID_BTN_OK)
            break;
    }
}

static int guiGamePadMacroUpdater(int modified)
{
    int previousSource = gPadMacroSource;
    diaGetInt(diaPadMacroConfig, PADMACRO_CFG_SOURCE, &gPadMacroSource);

    // update GUI to display per-game or global settings if changed
    if (previousSource != gPadMacroSource && gPadMacroSource == SETTINGS_GLOBAL) {
        config_set_t *configSet = gameMenuLoadConfig(diaPadMacroConfig);
        configRemoveKey(configSet, CONFIG_ITEM_PADMACROSOURCE);
        guiGameLoadPadMacroConfig(configSet, configGetByType(CONFIG_GAME));
    } else if (previousSource != gPadMacroSource && gPadMacroSource == SETTINGS_PERGAME) {
        config_set_t *configSet = gameMenuLoadConfig(diaPadMacroConfig);
        configSetInt(configSet, CONFIG_ITEM_PADMACROSOURCE, gPadMacroSource);
        guiGameLoadPadMacroConfig(configSet, configGetByType(CONFIG_GAME));
    }

    int slowdown_l, slowdown_r;
    diaGetInt(diaPadMacroConfig, PADMACRO_SLOWDOWN_L, &slowdown_l);
    PadMacroSettings.l_slowdown_enable = slowdown_l != 0 ? 1 : 0;
    PadMacroSettings.left_stick_slowdown = button_enum_to_bit_number[slowdown_l];

    diaGetInt(diaPadMacroConfig, PADMACRO_SLOWDOWN_R, &slowdown_r);
    PadMacroSettings.r_slowdown_enable = slowdown_r != 0 ? 1 : 0;
    PadMacroSettings.right_stick_slowdown = button_enum_to_bit_number[slowdown_r];

    int toggle_l, toggle_r;
    diaGetInt(diaPadMacroConfig, PADMACRO_SLOWDOWN_TOGGLE_L, &toggle_l);
    PadMacroSettings.l_slowdown_toggle = toggle_l;
    diaGetInt(diaPadMacroConfig, PADMACRO_SLOWDOWN_TOGGLE_R, &toggle_r);
    PadMacroSettings.r_slowdown_toggle = toggle_r;

    int lx_invert, ly_invert, rx_invert, ry_invert;
    diaGetInt(diaPadMacroConfig, PADMACRO_INVERT_LX, &lx_invert);
    PadMacroSettings.lx_invert = lx_invert;
    diaGetInt(diaPadMacroConfig, PADMACRO_INVERT_LY, &ly_invert);
    PadMacroSettings.ly_invert = ly_invert;
    diaGetInt(diaPadMacroConfig, PADMACRO_INVERT_RX, &rx_invert);
    PadMacroSettings.rx_invert = rx_invert;
    diaGetInt(diaPadMacroConfig, PADMACRO_INVERT_RY, &ry_invert);
    PadMacroSettings.ry_invert = ry_invert;

    int turbo_speed;
    diaGetInt(diaPadMacroConfig, PADMACRO_TURBO_SPEED, &turbo_speed);
    PadMacroSettings.turbo_speed = 4 - turbo_speed;

    return 0;
}

void guiGameShowPadMacroConfig(int forceGlobal)
{
    const char *settingsSource[] = {_l(_STR_GLOBAL_SETTINGS), _l(_STR_PERGAME_SETTINGS), NULL};
    forceGlobalPadMacro = forceGlobal;
    diaSetEnabled(diaPadMacroConfig, PADMACRO_CFG_SOURCE, !forceGlobalPadMacro);

    if (forceGlobalPadMacro) {
        guiGameLoadPadMacroConfig(NULL, configGetByType(CONFIG_GAME));
    }

    diaSetEnum(diaPadMacroConfig, PADMACRO_CFG_SOURCE, settingsSource);
    diaSetEnum(diaPadMacroConfig, PADMACRO_SLOWDOWN_L, button_names_enum);
    diaSetEnum(diaPadMacroConfig, PADMACRO_SLOWDOWN_R, button_names_enum);
    const char *toggle_enum[] = {_l(_STR_ACT_WHILE_PRESSED), _l(_STR_ACT_TOGGLED), NULL};
    diaSetEnum(diaPadMacroConfig, PADMACRO_SLOWDOWN_TOGGLE_L, toggle_enum);
    diaSetEnum(diaPadMacroConfig, PADMACRO_SLOWDOWN_TOGGLE_R, toggle_enum);

    int result = -1;
    while (result != 0) {
        result = diaExecuteDialog(diaPadMacroConfig, result, 1, &guiGamePadMacroUpdater);

        if (result == UIID_BTN_OK)
            break;
    }
}

static int guiGameSavePadEmuGameConfig(config_set_t *configSet, int result)
{
    if (gPadEmuSource == SETTINGS_PERGAME) {
        diaGetInt(diaPadEmuConfig, PADCFG_PADEMU_ENABLE, &EnablePadEmu);

        result = configSetInt(configSet, CONFIG_ITEM_PADEMUSOURCE, gPadEmuSource);
        if (EnablePadEmu != 0)
            result = configSetInt(configSet, CONFIG_ITEM_ENABLEPADEMU, EnablePadEmu);
        else
            configRemoveKey(configSet, CONFIG_ITEM_ENABLEPADEMU);

        if (PadEmuSettings != 0)
            result = configSetInt(configSet, CONFIG_ITEM_PADEMUSETTINGS, PadEmuSettings);
        else
            configRemoveKey(configSet, CONFIG_ITEM_PADEMUSETTINGS);
    }

    return result;
}

static int guiGameSavePadMacroGameConfig(config_set_t *configSet, int result)
{
    if (gPadMacroSource == SETTINGS_PERGAME) {
        result = configSetInt(configSet, CONFIG_ITEM_PADMACROSOURCE, gPadMacroSource);
        if (PadMacroSettings.raw != 0)
            result = configSetInt(configSet, CONFIG_ITEM_PADMACROSETTINGS, PadMacroSettings.raw);
        else
            configRemoveKey(configSet, CONFIG_ITEM_PADMACROSETTINGS);
    }

    return result;
}

void guiGameSavePadEmuGlobalConfig(config_set_t *configGame)
{
    if (gPadEmuSource == SETTINGS_GLOBAL) {
        configSetInt(configGame, CONFIG_ITEM_ENABLEPADEMU, EnablePadEmu);
        configSetInt(configGame, CONFIG_ITEM_PADEMUSETTINGS, PadEmuSettings);
    }
}

void guiGameSavePadMacroGlobalConfig(config_set_t *configGame)
{
    if (gPadMacroSource == SETTINGS_GLOBAL) {
        configSetInt(configGame, CONFIG_ITEM_PADMACROSETTINGS, PadMacroSettings.raw);
    }
}
#endif

void guiGameShowCompatConfig(int id, item_list_t *support, config_set_t *configSet)
{
    int i;

    if (support->flags & MODE_FLAG_COMPAT_DMA) {
        const char *dmaModes[] = {"MDMA 0", "MDMA 1", "MDMA 2", "UDMA 0", "UDMA 1", "UDMA 2", "UDMA 3", "UDMA 4", NULL};
        diaSetEnum(diaCompatConfig, COMPAT_DMA, dmaModes);
    } else {
        const char *dmaModes[] = {NULL};
        diaSetEnum(diaCompatConfig, COMPAT_DMA, dmaModes);
    }

    int result = diaExecuteDialog(diaCompatConfig, -1, 1, NULL);
    if (result) {
        compatMode = 0;
        for (i = 0; i < COMPAT_MODE_COUNT; ++i) {
            int mdpart;
            diaGetInt(diaCompatConfig, COMPAT_MODE_BASE + i, &mdpart);
            compatMode |= (mdpart ? 1 : 0) << i;
        }

        if (result == COMPAT_LOADFROMDISC) {
            if (sysGetDiscID(hexDiscID) >= 0)
                diaSetString(diaCompatConfig, COMPAT_GAMEID, hexDiscID);
            else
                guiMsgBox(_l(_STR_ERROR_LOADING_ID), 0, NULL);
        }

        if (result == COMPAT_DL_DEFAULTS)
            guiShowNetCompatUpdateSingle(id, support, configSet);

        diaGetInt(diaCompatConfig, COMPAT_DMA, &dmaMode);
        diaGetString(diaCompatConfig, COMPAT_GAMEID, hexid, sizeof(hexid));
        diaGetString(diaCompatConfig, COMPAT_ALTSTARTUP, altStartup, sizeof(altStartup));
    }
}

// sets variables without writing to users cfg file.. follow up with menuSaveConfig() to write
int guiGameSaveConfig(config_set_t *configSet, item_list_t *support)
{
    int i;
    int result = 0;
    config_set_t *configGame = configGetByType(CONFIG_GAME);

    compatMode = 0;
    for (i = 0; i < COMPAT_MODE_COUNT; ++i) {
        int mdpart;
        diaGetInt(diaCompatConfig, COMPAT_MODE_BASE + i, &mdpart);
        compatMode |= (mdpart ? 1 : 0) << i;
    }

    if (support->flags & MODE_FLAG_COMPAT_DMA) {
        diaGetInt(diaCompatConfig, COMPAT_DMA, &dmaMode);
        if (dmaMode != 7)
            result = configSetInt(configSet, CONFIG_ITEM_DMA, dmaMode);
        else
            configRemoveKey(configSet, CONFIG_ITEM_DMA);
    }

    if (compatMode != 0)
        result = configSetInt(configSet, CONFIG_ITEM_COMPAT, compatMode);
    else
        configRemoveKey(configSet, CONFIG_ITEM_COMPAT);

    /// GSM ///
    diaGetInt(diaGSConfig, GSMCFG_ENABLEGSM, &EnableGSM);
    diaGetInt(diaGSConfig, GSMCFG_GSMVMODE, &GSMVMode);
    diaGetInt(diaGSConfig, GSMCFG_GSMXOFFSET, &GSMXOffset);
    diaGetInt(diaGSConfig, GSMCFG_GSMYOFFSET, &GSMYOffset);
    diaGetInt(diaGSConfig, GSMCFG_GSMFIELDFIX, &GSMFIELDFix);

    if (gGSMSource == SETTINGS_PERGAME) {
        result = configSetInt(configSet, CONFIG_ITEM_GSMSOURCE, gGSMSource);
        if (EnableGSM != 0)
            result = configSetInt(configSet, CONFIG_ITEM_ENABLEGSM, EnableGSM);
        else
            configRemoveKey(configSet, CONFIG_ITEM_ENABLEGSM);

        if (GSMVMode != 0)
            result = configSetInt(configSet, CONFIG_ITEM_GSMVMODE, GSMVMode);
        else
            configRemoveKey(configSet, CONFIG_ITEM_GSMVMODE);

        if (GSMXOffset != 0)
            result = configSetInt(configSet, CONFIG_ITEM_GSMXOFFSET, GSMXOffset);
        else
            configRemoveKey(configSet, CONFIG_ITEM_GSMXOFFSET);

        if (GSMYOffset != 0)
            result = configSetInt(configSet, CONFIG_ITEM_GSMYOFFSET, GSMYOffset);
        else
            configRemoveKey(configSet, CONFIG_ITEM_GSMYOFFSET);

        if (GSMFIELDFix != 0)
            result = configSetInt(configSet, CONFIG_ITEM_GSMFIELDFIX, GSMFIELDFix);
        else
            configRemoveKey(configSet, CONFIG_ITEM_GSMFIELDFIX);
    } else if (gGSMSource == SETTINGS_GLOBAL) {
        configSetInt(configGame, CONFIG_ITEM_ENABLEGSM, EnableGSM);
        configSetInt(configGame, CONFIG_ITEM_GSMVMODE, GSMVMode);
        configSetInt(configGame, CONFIG_ITEM_GSMXOFFSET, GSMXOffset);
        configSetInt(configGame, CONFIG_ITEM_GSMYOFFSET, GSMYOffset);
        configSetInt(configGame, CONFIG_ITEM_GSMFIELDFIX, GSMFIELDFix);
    }

    /// Cheats ///
    diaGetInt(diaCheatConfig, CHTCFG_CHEATSOURCE, &gCheatSource);
    diaGetInt(diaCheatConfig, CHTCFG_ENABLECHEAT, &EnableCheat);
    diaGetInt(diaCheatConfig, CHTCFG_CHEATMODE, &CheatMode);

    if (gCheatSource == SETTINGS_PERGAME) {
        result = configSetInt(configSet, CONFIG_ITEM_CHEATSSOURCE, gCheatSource);
        if (EnableCheat != 0)
            result = configSetInt(configSet, CONFIG_ITEM_ENABLECHEAT, EnableCheat);
        else
            configRemoveKey(configSet, CONFIG_ITEM_ENABLECHEAT);

        if (CheatMode != 0)
            result = configSetInt(configSet, CONFIG_ITEM_CHEATMODE, CheatMode);
        else
            configRemoveKey(configSet, CONFIG_ITEM_CHEATMODE);
    } else if (gCheatSource == SETTINGS_GLOBAL) {
        configSetInt(configGame, CONFIG_ITEM_ENABLECHEAT, EnableCheat);
        configSetInt(configGame, CONFIG_ITEM_CHEATMODE, CheatMode);
    }

#ifdef PADEMU
    /// PADEMU ///
    result = guiGameSavePadEmuGameConfig(configSet, result);
    guiGameSavePadEmuGlobalConfig(configGame);
    result = guiGameSavePadMacroGameConfig(configSet, result);
    guiGameSavePadMacroGlobalConfig(configGame);
#endif

    diaGetString(diaCompatConfig, COMPAT_GAMEID, hexid, sizeof(hexid));
    if (hexid[0] != '\0')
        result = configSetStr(configSet, CONFIG_ITEM_DNAS, hexid);

    diaGetString(diaCompatConfig, COMPAT_ALTSTARTUP, altStartup, sizeof(altStartup));
    if (altStartup[0] != '\0')
        result = configSetStr(configSet, CONFIG_ITEM_ALTSTARTUP, altStartup);
    else
        configRemoveKey(configSet, CONFIG_ITEM_ALTSTARTUP);

    /// Parental lock ///
    int parentalLock = 0;
    diaGetInt(diaCompatConfig, COMPAT_PARENTAL_LOCK, &parentalLock);
    if (parentalLock != 0) {
        result = configSetInt(configSet, CONFIG_ITEM_PARENTAL_LOCK, 1);
        // The lock is only enforced while a parental lock password exists, so warn instead of silently saving a lock that does nothing.
        if (!menuIsParentalLockPasswordSet())
            guiMsgBox(_l(_STR_PARENLOCK_GAME_NO_PASSWORD), 0, NULL);
    } else
        configRemoveKey(configSet, CONFIG_ITEM_PARENTAL_LOCK);

    /// VMC ///
    configSetVMC(configSet, vmc1, 0);
    configSetVMC(configSet, vmc2, 1);

    result = guiGameSaveOSDLanguageGameConfig(configSet, result);
    guiGameSaveOSDLanguageGlobalConfig(configGame);

    return result;
}

void guiGameRemoveGlobalSettings(config_set_t *configGame)
{
    if (menuCheckParentalLock() == 0) {
        // Cheats
        configRemoveKey(configGame, CONFIG_ITEM_ENABLECHEAT);
        configRemoveKey(configGame, CONFIG_ITEM_CHEATMODE);

        // GSM
        configRemoveKey(configGame, CONFIG_ITEM_ENABLEGSM);
        configRemoveKey(configGame, CONFIG_ITEM_GSMVMODE);
        configRemoveKey(configGame, CONFIG_ITEM_GSMXOFFSET);
        configRemoveKey(configGame, CONFIG_ITEM_GSMYOFFSET);
        configRemoveKey(configGame, CONFIG_ITEM_GSMFIELDFIX);
        // OSD Language
        configRemoveKey(configGame, CONFIG_ITEM_OSD_SETTINGS_LANGID);
        configRemoveKey(configGame, CONFIG_ITEM_OSD_SETTINGS_TV_ASP);
        configRemoveKey(configGame, CONFIG_ITEM_OSD_SETTINGS_VMODE);
        configRemoveKey(configGame, CONFIG_ITEM_OSD_SETTINGS_ENABLE);

#ifdef PADEMU
        // PADEMU
        configRemoveKey(configGame, CONFIG_ITEM_ENABLEPADEMU);
        configRemoveKey(configGame, CONFIG_ITEM_PADEMUSETTINGS);
        configRemoveKey(configGame, CONFIG_ITEM_PADMACROSETTINGS);
#endif
        saveConfig(CONFIG_GAME, 0);
    }
}

void guiGameRemoveSettings(config_set_t *configSet)
{
    if (menuCheckParentalLock() == 0) {
        configRemoveKey(configSet, CONFIG_ITEM_CONFIGSOURCE);
        configRemoveKey(configSet, CONFIG_ITEM_DMA);
        configRemoveKey(configSet, CONFIG_ITEM_COMPAT);
        configRemoveKey(configSet, CONFIG_ITEM_DNAS);
        configRemoveKey(configSet, CONFIG_ITEM_ALTSTARTUP);
        configRemoveKey(configSet, CONFIG_ITEM_PARENTAL_LOCK);

        // GSM
        configRemoveKey(configSet, CONFIG_ITEM_GSMSOURCE);
        configRemoveKey(configSet, CONFIG_ITEM_ENABLEGSM);
        configRemoveKey(configSet, CONFIG_ITEM_GSMVMODE);
        configRemoveKey(configSet, CONFIG_ITEM_GSMXOFFSET);
        configRemoveKey(configSet, CONFIG_ITEM_GSMYOFFSET);
        configRemoveKey(configSet, CONFIG_ITEM_GSMFIELDFIX);

        // Cheats
        configRemoveKey(configSet, CONFIG_ITEM_CHEATSSOURCE);
        configRemoveKey(configSet, CONFIG_ITEM_ENABLECHEAT);
        configRemoveKey(configSet, CONFIG_ITEM_CHEATMODE);

        // OSD Language
        configRemoveKey(configSet, CONFIG_ITEM_OSD_SETTINGS_LANGID);
        configRemoveKey(configSet, CONFIG_ITEM_OSD_SETTINGS_TV_ASP);
        configRemoveKey(configSet, CONFIG_ITEM_OSD_SETTINGS_VMODE);
        configRemoveKey(configSet, CONFIG_ITEM_OSD_SETTINGS_SOURCE);
        configRemoveKey(configSet, CONFIG_ITEM_OSD_SETTINGS_ENABLE);

#ifdef PADEMU
        // PADEMU
        configRemoveKey(configSet, CONFIG_ITEM_PADEMUSOURCE);
        configRemoveKey(configSet, CONFIG_ITEM_ENABLEPADEMU);
        configRemoveKey(configSet, CONFIG_ITEM_PADEMUSETTINGS);
        configRemoveKey(configSet, CONFIG_ITEM_PADMACROSETTINGS);
        configRemoveKey(configSet, CONFIG_ITEM_PADMACROSOURCE);
#endif
        // VMC
        configRemoveVMC(configSet, 0);
        configRemoveVMC(configSet, 1);

        menuSaveConfig();
    }
}

void guiGameTestSettings(int id, item_list_t *support, config_set_t *configSet)
{
    guiGameSaveConfig(configSet, support);
    support->itemLaunch(support, id, configSet);
}

static void guiGameLoadGSMConfig(config_set_t *configSet, config_set_t *configGame)
{
    EnableGSM = 0;
    GSMVMode = 0;
    GSMXOffset = 0;
    GSMYOffset = 0;
    GSMFIELDFix = 0;

    // set global settings.
    gGSMSource = 0;
    configGetInt(configGame, CONFIG_ITEM_ENABLEGSM, &EnableGSM);
    configGetInt(configGame, CONFIG_ITEM_GSMVMODE, &GSMVMode);
    configGetInt(configGame, CONFIG_ITEM_GSMXOFFSET, &GSMXOffset);
    configGetInt(configGame, CONFIG_ITEM_GSMYOFFSET, &GSMYOffset);
    configGetInt(configGame, CONFIG_ITEM_GSMFIELDFIX, &GSMFIELDFix);

    // override global with per-game settings if available and selected.
    configGetInt(configSet, CONFIG_ITEM_GSMSOURCE, &gGSMSource);
    if (gGSMSource == SETTINGS_PERGAME) {
        if (!configGetInt(configSet, CONFIG_ITEM_ENABLEGSM, &EnableGSM))
            EnableGSM = 0;
        if (!configGetInt(configSet, CONFIG_ITEM_GSMVMODE, &GSMVMode))
            GSMVMode = 0;
        if (!configGetInt(configSet, CONFIG_ITEM_GSMXOFFSET, &GSMXOffset))
            GSMXOffset = 0;
        if (!configGetInt(configSet, CONFIG_ITEM_GSMYOFFSET, &GSMYOffset))
            GSMYOffset = 0;
        if (!configGetInt(configSet, CONFIG_ITEM_GSMFIELDFIX, &GSMFIELDFix))
            GSMFIELDFix = 0;
    }

    // set gui settings.
    diaSetInt(diaGSConfig, GSMCFG_GSMSOURCE, gGSMSource);
    diaSetInt(diaGSConfig, GSMCFG_ENABLEGSM, EnableGSM);
    diaSetInt(diaGSConfig, GSMCFG_GSMVMODE, GSMVMode);
    diaSetInt(diaGSConfig, GSMCFG_GSMXOFFSET, GSMXOffset);
    diaSetInt(diaGSConfig, GSMCFG_GSMYOFFSET, GSMYOffset);
    diaSetInt(diaGSConfig, GSMCFG_GSMFIELDFIX, GSMFIELDFix);
}

static void guiGameLoadCheatsConfig(config_set_t *configSet, config_set_t *configGame)
{
    EnableCheat = 0;
    CheatMode = 0;

    // set global settings.
    gCheatSource = 0;
    configGetInt(configGame, CONFIG_ITEM_ENABLECHEAT, &EnableCheat);
    configGetInt(configGame, CONFIG_ITEM_CHEATMODE, &CheatMode);

    // override global with per-game settings if available and selected.
    configGetInt(configSet, CONFIG_ITEM_CHEATSSOURCE, &gCheatSource);
    if (gCheatSource == SETTINGS_PERGAME) {
        if (!configGetInt(configSet, CONFIG_ITEM_ENABLECHEAT, &EnableCheat))
            EnableCheat = 0;
        if (!configGetInt(configSet, CONFIG_ITEM_CHEATMODE, &CheatMode))
            CheatMode = 0;
    }

    // set gui settings.
    diaSetInt(diaCheatConfig, CHTCFG_CHEATSOURCE, gCheatSource);
    diaSetInt(diaCheatConfig, CHTCFG_ENABLECHEAT, EnableCheat);
    diaSetInt(diaCheatConfig, CHTCFG_CHEATMODE, CheatMode);
}

#ifdef PADEMU
static void guiGameLoadPadEmuConfig(config_set_t *configSet, config_set_t *configGame)
{
    EnablePadEmu = 0;
    PadEmuSettings = 0;

    // set global settings.
    gPadEmuSource = 0;
    configGetInt(configGame, CONFIG_ITEM_ENABLEPADEMU, &EnablePadEmu);
    configGetInt(configGame, CONFIG_ITEM_PADEMUSETTINGS, &PadEmuSettings);

    // override global with per-game settings if available and selected.
    if (!forceGlobalPadEmu) {
        configGetInt(configSet, CONFIG_ITEM_PADEMUSOURCE, &gPadEmuSource);
        if (gPadEmuSource == SETTINGS_PERGAME) {
            if (!configGetInt(configSet, CONFIG_ITEM_ENABLEPADEMU, &EnablePadEmu))
                EnablePadEmu = 0;
            if (!configGetInt(configSet, CONFIG_ITEM_PADEMUSETTINGS, &PadEmuSettings))
                PadEmuSettings = 0;
        }
    }
    // set gui settings.
    int PadEmuMtap = (PadEmuSettings >> 24) & 1;
    int PadEmuMtapPort = ((PadEmuSettings >> 25) & 1) + 1;

    diaSetInt(diaPadEmuConfig, PADCFG_PADEMU_SOURCE, gPadEmuSource);
    diaSetInt(diaPadEmuConfig, PADCFG_PADEMU_ENABLE, EnablePadEmu);

    diaSetInt(diaPadEmuConfig, PADCFG_PADEMU_MODE, PadEmuSettings & 0xFF);
    for (int slot = 0; slot < 4; slot++) {
        int shown = PadEmuMtap || slot < 2;
        int emu = shown ? (PadEmuSettings >> (8 + slot)) & 1 : 0;
        int vib = shown ? (PadEmuSettings >> (16 + slot)) & 1 : 0;

        // one choice per slot: Off / On / On + Vibration (vibration without emulation does nothing)
        diaSetInt(diaPadEmuConfig, PADCFG_SLOT_EMU0 + slot, !emu ? SLOT_OFF : (vib ? SLOT_ON_VIBRATE : SLOT_ON));
    }
    diaSetInt(diaPadEmuConfig, PADCFG_PADEMU_MTAP, PadEmuMtap);
    diaSetInt(diaPadEmuConfig, PADCFG_PADEMU_MTAP_PORT, PadEmuMtapPort);
    diaSetInt(diaPadEmuBtPads, PADCFG_PADEMU_WORKAROUND, ((PadEmuSettings >> 26) & 1));
}

static void guiGameLoadPadMacroConfig(config_set_t *configSet, config_set_t *configGame)
{
    PadMacroSettings.raw = 0;
    gPadMacroSource = 0;

    configGetInt(configGame, CONFIG_ITEM_PADMACROSETTINGS, &PadMacroSettings.raw);
    // override global with per-game settings if available and selected.
    if (!forceGlobalPadMacro) {
        configGetInt(configSet, CONFIG_ITEM_PADMACROSOURCE, &gPadMacroSource);
        if (gPadMacroSource == SETTINGS_PERGAME) {
            if (!configGetInt(configSet, CONFIG_ITEM_PADMACROSETTINGS, &PadMacroSettings.raw))
                PadMacroSettings.raw = 0;
        }
    }

    diaSetInt(diaPadMacroConfig, PADMACRO_CFG_SOURCE, gPadMacroSource);

    int slowdown_l_ui = 0, slowdown_r_ui = 0;
    if (PadMacroSettings.l_slowdown_enable) {
        slowdown_l_ui = bit_number_to_button_enum[PadMacroSettings.left_stick_slowdown];
    }

    if (PadMacroSettings.r_slowdown_enable) {
        slowdown_r_ui = bit_number_to_button_enum[PadMacroSettings.right_stick_slowdown];
    }

    diaSetInt(diaPadMacroConfig, PADMACRO_SLOWDOWN_L, slowdown_l_ui);
    diaSetInt(diaPadMacroConfig, PADMACRO_SLOWDOWN_R, slowdown_r_ui);
    diaSetInt(diaPadMacroConfig, PADMACRO_SLOWDOWN_TOGGLE_L, PadMacroSettings.l_slowdown_toggle);
    diaSetInt(diaPadMacroConfig, PADMACRO_SLOWDOWN_TOGGLE_R, PadMacroSettings.r_slowdown_toggle);
    diaSetInt(diaPadMacroConfig, PADMACRO_INVERT_LX, PadMacroSettings.lx_invert);
    diaSetInt(diaPadMacroConfig, PADMACRO_INVERT_LY, PadMacroSettings.ly_invert);
    diaSetInt(diaPadMacroConfig, PADMACRO_INVERT_RX, PadMacroSettings.rx_invert);
    diaSetInt(diaPadMacroConfig, PADMACRO_INVERT_RY, PadMacroSettings.ry_invert);
    diaSetInt(diaPadMacroConfig, PADMACRO_TURBO_SPEED, 4 - PadMacroSettings.turbo_speed);
}
#endif

// OSD

static int guiGameOSDLanguageUpdater(int modified)
{
    int previousSource = gOSDLanguageSource;
    diaGetInt(diaOSDConfig, OSD_LANGUAGE_SOURCE, &gOSDLanguageSource);

    // update GUI to display per-game or global settings if changed
    if (previousSource != gOSDLanguageSource && gOSDLanguageSource == SETTINGS_GLOBAL) {
        config_set_t *configSet = gameMenuLoadConfig(diaOSDConfig);
        configRemoveKey(configSet, CONFIG_ITEM_OSD_SETTINGS_SOURCE);
        guiGameLoadOSDLanguageConfig(configSet, configGetByType(CONFIG_GAME));
    } else if (previousSource != gOSDLanguageSource && gOSDLanguageSource == SETTINGS_PERGAME) {
        config_set_t *configSet = gameMenuLoadConfig(diaOSDConfig);
        configSetInt(configSet, CONFIG_ITEM_OSD_SETTINGS_SOURCE, gOSDLanguageSource);
        guiGameLoadOSDLanguageConfig(configSet, configGetByType(CONFIG_GAME));
    }

    diaGetInt(diaOSDConfig, OSD_LANGUAGE_ENABLE, &gOSDLanguageEnable);
    diaGetInt(diaOSDConfig, OSD_LANGUAGE_VALUE, &gOSDLanguageValue);
    diaGetInt(diaOSDConfig, OSD_TVASPECT_VALUE, &gOSDTVAspectRatio);
    diaGetInt(diaOSDConfig, OSD_VMODE_VALUE, &gOSDVideOutput);
    diaSetEnabled(diaOSDConfig, OSD_LANGUAGE_VALUE, gOSDLanguageEnable);
    diaSetEnabled(diaOSDConfig, OSD_TVASPECT_VALUE, gOSDLanguageEnable);
    diaSetEnabled(diaOSDConfig, OSD_VMODE_VALUE, gOSDLanguageEnable);
    return 0;
}

void guiGameShowOSDLanguageConfig(int forceGlobal)
{
    /// `_STR_SYSTEM_DEFAULT` MUST BE AT THE END OF THE LIST. ELSE, THE ENUMERATOR VALUES WILL NOT MATCH THE ONES ON OSDCONFIG. BREAKING THIS FEATURE
    const char *Lngs[] = {_l(_STR_LANGUAGE_JAPANESE), _l(_STR_LANGUAGE_ENGLISH), _l(_STR_LANGUAGE_FRENCH), _l(_STR_LANGUAGE_SPANISH), _l(_STR_LANGUAGE_GERMAN), _l(_STR_LANGUAGE_ITALIAN), _l(_STR_LANGUAGE_DUTCH), _l(_STR_LANGUAGE_PORTUGUESE), _l(_STR_SYSTEM_DEFAULT), NULL};
    const char *sources[] = {_l(_STR_GLOBAL_SETTINGS), _l(_STR_PERGAME_SETTINGS), NULL};
    const char *TV[] = {"4:3", _l(_STR_FULL_SCREEN), "16:9", _l(_STR_SYSTEM_DEFAULT), NULL};
    const char *VMOD[] = {"RGB", "Y Cb/Pb Cr/Pr", _l(_STR_SYSTEM_DEFAULT), NULL};
    diaSetEnum(diaOSDConfig, OSD_LANGUAGE_VALUE, Lngs);
    diaSetEnum(diaOSDConfig, OSD_LANGUAGE_SOURCE, sources);
    diaSetEnum(diaOSDConfig, OSD_TVASPECT_VALUE, TV);
    diaSetEnum(diaOSDConfig, OSD_VMODE_VALUE, VMOD);
    forceGlobalOSDLanguage = forceGlobal;
    diaSetEnabled(diaOSDConfig, OSD_LANGUAGE_SOURCE, !forceGlobalOSDLanguage);

    if (forceGlobalOSDLanguage) {
        guiGameLoadOSDLanguageConfig(NULL, configGetByType(CONFIG_GAME));
    }


    int result = -1;
    while (result != 0) {
        result = diaExecuteDialog(diaOSDConfig, result, 1, &guiGameOSDLanguageUpdater);

        if (result == UIID_BTN_OK)
            break;
    }
}
static int guiGameSaveOSDLanguageGameConfig(config_set_t *configSet, int result)
{
    if (gOSDLanguageSource == SETTINGS_PERGAME) {
        if ((result = configSetInt(configSet, CONFIG_ITEM_OSD_SETTINGS_SOURCE, gOSDLanguageSource)))
            if ((result = configSetInt(configSet, CONFIG_ITEM_OSD_SETTINGS_ENABLE, gOSDLanguageEnable))) {
                configSetInt(configSet, CONFIG_ITEM_OSD_SETTINGS_LANGID, gOSDLanguageValue);
                configSetInt(configSet, CONFIG_ITEM_OSD_SETTINGS_TV_ASP, gOSDTVAspectRatio);
                result = configSetInt(configSet, CONFIG_ITEM_OSD_SETTINGS_VMODE, gOSDVideOutput);
            }
    } else {
        configRemoveKey(configSet, CONFIG_ITEM_OSD_SETTINGS_LANGID);
        configRemoveKey(configSet, CONFIG_ITEM_OSD_SETTINGS_TV_ASP);
        configRemoveKey(configSet, CONFIG_ITEM_OSD_SETTINGS_VMODE);
        configRemoveKey(configSet, CONFIG_ITEM_OSD_SETTINGS_ENABLE);
    }

    return result;
}

void guiGameSaveOSDLanguageGlobalConfig(config_set_t *configGame)
{
    if (gOSDLanguageSource == SETTINGS_GLOBAL) {
        configSetInt(configGame, CONFIG_ITEM_OSD_SETTINGS_ENABLE, gOSDLanguageEnable);
        configSetInt(configGame, CONFIG_ITEM_OSD_SETTINGS_LANGID, gOSDLanguageValue);
        configSetInt(configGame, CONFIG_ITEM_OSD_SETTINGS_TV_ASP, gOSDTVAspectRatio);
        configSetInt(configGame, CONFIG_ITEM_OSD_SETTINGS_VMODE, gOSDVideOutput);
    }
}

static void guiGameLoadOSDLanguageConfig(config_set_t *configSet, config_set_t *configGame)
{
    gOSDLanguageValue = 0;
    gOSDLanguageEnable = 0;
    gOSDLanguageSource = 0;
    gOSDTVAspectRatio = 0;
    gOSDVideOutput = 0;

    configGetInt(configGame, CONFIG_ITEM_OSD_SETTINGS_ENABLE, &gOSDLanguageEnable);
    configGetInt(configGame, CONFIG_ITEM_OSD_SETTINGS_LANGID, &gOSDLanguageValue);
    configGetInt(configGame, CONFIG_ITEM_OSD_SETTINGS_TV_ASP, &gOSDTVAspectRatio);
    configGetInt(configGame, CONFIG_ITEM_OSD_SETTINGS_VMODE, &gOSDVideOutput);
    // override global with per-game settings if available and selected.
    if (!forceGlobalOSDLanguage) {
        configGetInt(configSet, CONFIG_ITEM_OSD_SETTINGS_SOURCE, &gOSDLanguageSource);
        if (gOSDLanguageSource == SETTINGS_PERGAME) {
            if (!configGetInt(configSet, CONFIG_ITEM_OSD_SETTINGS_ENABLE, &gOSDLanguageEnable))
                gOSDLanguageEnable = 0;
            if (!configGetInt(configSet, CONFIG_ITEM_OSD_SETTINGS_LANGID, &gOSDLanguageValue))
                gOSDLanguageValue = 0;
            if (!configGetInt(configSet, CONFIG_ITEM_OSD_SETTINGS_TV_ASP, &gOSDTVAspectRatio))
                gOSDTVAspectRatio = 0;
            if (!configGetInt(configSet, CONFIG_ITEM_OSD_SETTINGS_VMODE, &gOSDVideOutput))
                gOSDVideOutput = 0;
        }
    }

    diaSetInt(diaOSDConfig, OSD_LANGUAGE_SOURCE, gOSDLanguageSource);
    diaSetInt(diaOSDConfig, OSD_LANGUAGE_ENABLE, gOSDLanguageEnable);
    diaSetInt(diaOSDConfig, OSD_LANGUAGE_VALUE, gOSDLanguageValue);
    diaSetInt(diaOSDConfig, OSD_TVASPECT_VALUE, gOSDTVAspectRatio);
    diaSetInt(diaOSDConfig, OSD_VMODE_VALUE, gOSDVideOutput);
}
// OSD Language

// loads defaults if no config found
void guiGameLoadConfig(item_list_t *support, config_set_t *configSet)
{
    int i;
    config_set_t *configGame = configGetByType(CONFIG_GAME);

    configSource[0] = '\0';
    configSourceID = CONFIG_SOURCE_DEFAULT;
    configGetInt(configSet, CONFIG_ITEM_CONFIGSOURCE, &configSourceID);
    if (configSourceID == CONFIG_SOURCE_USER)
        snprintf(configSource, sizeof(configSource), _l(_STR_CUSTOMIZED_SETTINGS));
    else if (configSourceID == CONFIG_SOURCE_DLOAD)
        snprintf(configSource, sizeof(configSource), _l(_STR_DOWNLOADED_DEFAULTS));

    dmaMode = 7; // defaulting to UDMA 4
    if (support->flags & MODE_FLAG_COMPAT_DMA) {
        configGetInt(configSet, CONFIG_ITEM_DMA, &dmaMode);
        diaSetInt(diaCompatConfig, COMPAT_DMA, dmaMode);
    } else
        diaSetInt(diaCompatConfig, COMPAT_DMA, 0);

    compatMode = 0;
    configGetInt(configSet, CONFIG_ITEM_COMPAT, &compatMode);
    for (i = 0; i < COMPAT_MODE_COUNT; ++i)
        diaSetInt(diaCompatConfig, COMPAT_MODE_BASE + i, (compatMode & (1 << i)) > 0 ? 1 : 0);

    guiGameLoadGSMConfig(configSet, configGame);

    guiGameLoadCheatsConfig(configSet, configGame);
#ifdef PADEMU
    guiGameLoadPadEmuConfig(configSet, configGame);
    guiGameLoadPadMacroConfig(configSet, configGame);
#endif

    guiGameLoadOSDLanguageConfig(configSet, configGame);

    /// Find out the current game ID ///
    hexid[0] = '\0';
    configGetStrCopy(configSet, CONFIG_ITEM_DNAS, hexid, sizeof(hexid));
    diaSetString(diaCompatConfig, COMPAT_GAMEID, hexid);

    altStartup[0] = '\0';
    configGetStrCopy(configSet, CONFIG_ITEM_ALTSTARTUP, altStartup, sizeof(altStartup));
    diaSetString(diaCompatConfig, COMPAT_ALTSTARTUP, altStartup);

    /// Parental lock ///
    int parentalLock = 0;
    configGetInt(configSet, CONFIG_ITEM_PARENTAL_LOCK, &parentalLock);
    diaSetInt(diaCompatConfig, COMPAT_PARENTAL_LOCK, parentalLock ? 1 : 0);

    /// VMC ///
    vmc1[0] = '\0';
    configGetVMC(configSet, vmc1, sizeof(vmc1), 0);

    vmc2[0] = '\0';
    configGetVMC(configSet, vmc2, sizeof(vmc2), 1);
}
