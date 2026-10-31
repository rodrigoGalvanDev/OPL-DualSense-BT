#ifndef __GUIGAME_H
#define __GUIGAME_H

#define SETTINGS_GLOBAL  0
#define SETTINGS_PERGAME 1

int guiGameAltStartupNameHandler(char *text, int maxLen);

char *gameConfigSource(void);

int guiGameVmcNameHandler(char *text, int maxLen);
void guiGameShowVMCMenu(int id, item_list_t *support);
void guiGameShowCompatConfig(int id, item_list_t *support, config_set_t *configSet);
void guiGameShowGSConfig(void);
void guiGameShowCheatConfig(void);

#ifdef PADEMU
void guiGameShowPadEmuConfig(int forceGlobal);
void guiGameShowPadMacroConfig(int forceGlobal);
void guiGameSavePadEmuGlobalConfig(config_set_t *configGame);
void guiGameSavePadMacroGlobalConfig(config_set_t *configGame);
void guiGameBtKeysPushToDriver(void);
void guiGameBtKeysRequestPush(void);
void guiGameBtKeysPushIfPending(void);
void guiGameBtKeysModuleArg(char *out); // needs BTKEYS_ARG_LEN bytes
void guiGameSetupModuleArg(unsigned char *out); // Controller Setup blob, PADSETUP_ARG_LEN bytes
// Controller Setup (PADEMU), table driven by padsetup_params[] in include/padsetup.h
int guiGameSetupGet(int param);                  // PADSETUP_P_*; a stored value out of range reads as the default
int guiGameSetupSet(int param, int value);       // 1 = stored, 0 = out of range (unchanged)
int guiGameSetupGetRemap(int button);            // button 0..15 (DS2 bit number); UI index: 0 unchanged, 1 disabled, 2.. target
void guiGameSetupSetRemap(int button, int index);
void guiGameSetupResetDefaults(void);
void guiGameSetupCommit(void);                   // save to the OPL config + apply what the menu driver can use right away
void guiGameBtLedApply(int slot);               // send the lightbar colour of the slot to the menu driver
void guiGameBtLedOnConnect(int slot);           // a pad connected to the menu driver: find which saved pad it is, apply its colour if one was chosen
void guiGameBtLedOnDisconnect(int slot);        // the pad of the slot is gone
int guiGameBtLedGet(int slot, unsigned char *rgb); // colour chosen for the slot, 0 = default
#endif

void guiGameShowOSDLanguageConfig(int forceGlobal);
void guiGameSaveOSDLanguageGlobalConfig(config_set_t *configGame);

void guiGameLoadConfig(item_list_t *support, config_set_t *configSet);
int guiGameSaveConfig(config_set_t *configSet, item_list_t *support);
void guiGameTestSettings(int id, item_list_t *support, config_set_t *configSet);

void guiGameRemoveSettings(config_set_t *configSet);
void guiGameRemoveGlobalSettings(config_set_t *configGame);
#endif
