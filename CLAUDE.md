# CLAUDE.md

Context for working on **OPL-DualSense-BT**: a fork of Open PS2 Loader that adds DualSense (and DS4) support over Bluetooth to PADEMU, plus wireless pairing from the OPL menu.

- Repo: https://github.com/rodrigoGalvanDev/OPL-DualSense-BT (maintainer: Rodrigo, `rodrigoGalvanDev`)
- Collaborator: Miguel Queiroz (`MiguelQueiroz010`, a.k.a. Bit.Raiden), author of the fork OPL-DS4RTBT. **PR #1 is pending review** (see the end of this file).
- Current release: **v1.1**.

## Ground rules

- **Don't break what works.** Rodrigo's words: "I'd rather it take longer to connect than break something." Prefer small, conservative changes. When a feature is off, behaviour must be identical to before.
- **Everything is verified on real hardware.** Emulators don't cover the USB/Bluetooth path. Build → Rodrigo tests on his PS2 → only then merge or release.
- **IOP RAM is tight in-game.** Every byte in `bt_pademu.irx` and in the in-game modules costs game compatibility (e.g. KH2 over SMB ran out of IOP memory). Keep in-game additions minimal; big tables and blobs belong in the menu module.
- `ee_core` has no libc: no `strcpy` or `memcpy` (copy by hand).
- Two ELFs per release: release (no logs) and debug (network logs).

## Test hardware

| What | Model |
|---|---|
| Console | PS2 Slim SCPH-90001, Matrix modchip. R1 at boot is taken by the chip; Triangle opens uLE. |
| Dongle | Realtek RTL8761BU, USB `0BDA:8771`. Works from its ROM firmware. |
| Pads | DualSense (main). Original DS4 confirmed by a user. Wired DualShock 2 on port 2 for mixed tests. |
| Boot | OPL autoboots from the memory card (`mc0:/APPS/OPL.ELF`); games load over USB (BDM). |
| IGR | L1+L2+R1+R2+Start+Select returns to OPL (IGR Path = `mc0:/APPS/OPL.ELF`). L1+L2+R1+R2+L3+R3 powers off. |

## Repo and branches

- `dualsense-bt`: main branch, default on GitHub. Releases are built from here.
- `dualsense-bt-debug`: same code plus latency instrumentation (LAT lines) and `tools/lag_monitor.py`. Merge `dualsense-bt` into it periodically. Conflicts usually show up in `src/system.c` and `ds34bt.c`; keep both sides.
- `master`: tracks upstream.
- Remotes: `origin` = the fork, `upstream` = ps2homebrew/Open-PS2-Loader (we're based on its commit `4cc74ba`).
- **Upstream won't take LLM-assisted changes.** The ps2homebrew folks pointed us to RiptOPL (NathanNeurotic/Open-PS2-Loader) instead. Don't prepare an upstream PR without asking first.

## Building

You need the ps2dev toolchain, with `PS2DEV`, `PS2SDK` and `GSKIT` exported and the toolchain bins on `PATH`.

```sh
make download_lng          # once, so the language step doesn't fail
make                       # release
make DEBUG=1 INGAME_DEBUG=1 "CDVDMAN_DEBUG_FLAGS=USE_DEV9=1"   # debug with in-game network logs
```

- **Packing gotcha:** the final `ps2-packer` step can fail with "Error 255" when run from the repo, because it can't find its module/stub. `opl_stripped.elf` is still fine. Pack it by hand:
  ```sh
  cd $PS2DEV/share/ps2-packer/module && ps2-packer -s $PS2DEV/share/ps2-packer/stub/lzma-1d00-stub /path/to/opl_stripped.elf OUT.ELF
  ```
- **Debug builds and games over USB:** `CDVDMAN_DEBUG_FLAGS=IOPCORE_DEBUG=1` (the DEBUG default) makes cdvdman noisy. Leaving the variable empty removes DEV9, so in-game network logs stop arriving. Use `USE_DEV9=1` to get a quiet cdvdman *and* in-game logs.
- After changing `modules/ds34bt/ee`, do a clean build. A stale `libds34bt.a` once broke a build.

## Architecture (what we changed and why)

There are **two separate Bluetooth drivers**. Every fix usually has to go into both.

| | Menu | In-game |
|---|---|---|
| File | `modules/ds34bt/iop/ds34bt.c` (+ EE RPC lib in `modules/ds34bt/ee/`) | `modules/pademu/ds34bt.c` (built into `bt_pademu.irx`) |
| Pads | `MAX_PADS` 2, only slot 0 used in the menu | `MAX_PADS` 4 |
| Talks to | the EE over RPC | the game, via the SIO2 hook in `pademu.c` |

Launching a game resets the PS2's USB, so the pad **always** disconnects and the user presses PS again. That's expected, not a bug, and it's in the README.

### DualSense support
- Input report `0x31` is parsed into the DS2 report. The output report (rumble/lightbar) needs a **CRC32**. We use a nibble-table CRC (small and fast, no 1 KB table).
- **Rumble is only sent when it changes.** Games call set-rumble every frame. Sending a report each time came to ~63 output reports/s on the radio; now it's ~3/s.
- **Mini SDP server:** the DualSense queries the host's services and drops the link after ~30 s if nobody answers.
- USB pairing for the DualSense (`modules/ds34usb/iop/ds34usb.c`) works like the DS4 one, with DS5 feature reports `0x09`/`0x0A`/`0x08`.

### Wireless pairing (Secure Simple Pairing)
Flow in the menu driver:
1. Set Event Mask `FF FF FF FF FF 1F 27 00`, then Write SSP Mode, then Inquiry (LAP 0x9E8B33).
2. On Inquiry Result (`0x02`/`0x22`), the inquiry is **cancelled as soon as the first peripheral shows up** (`cod[1]&0x1F==5`); that's the speed-up. Then Remote Name Request.
3. Create Connection → Auth Requested → Link Key Negative Reply → IO Capability Reply (`03 00 04`, NoInputNoOutput) → User Confirm Reply.
4. Link Key Notification (event `0x18`) → Set Encryption, then the normal L2CAP flow.

How the link key gets around:
- **Format:** `include/btkeys.h`, the text "BTK" + 44 hex chars per pad (6-byte bdaddr in HCI order + 16-byte key). Up to 4 pads; a 5th drops the oldest.
- **Saved in:** `conf_opl.cfg`, key `pademu_bt_keys`.
- **Into the game:** `EECoreConfig_t.PadEmuBtKeys[180]` → `ee_core/src/iopmgr.c` builds the pademu args (8 settings bytes + key string) → `ds34bt_set_keys()`.
- **Cached in memory** (`btKeysCount` in `src/guigame.c`), because OPL frees its config (`configEnd`) before launching a game. This was the "0 wireless keys in game" bug.
- In-game, the SSP init chain only runs if keys exist. On Link Key Request it answers with the stored key, else the legacy fixed OPL key, so DS3/DS4 paired by USB still work.

UI: `PADEMU → Wireless Pairing → Search` (`src/dialogs.c`, `src/guigame.c`), with a 45 s timeout. You can't leave the dialog while pairing. The mode label reads `DS3/DS4/DS5 BT`.

### Page scan off while connected (the v1.1 latency fix)
`bt_update_page_scan()` in `modules/pademu/ds34bt.c`. Once every *enabled* pad is RUNNING, it sends Write Scan Enable = `NOINQ_NOPAG`. As soon as one disconnects, it goes back to `NOINQ_ENPAG` so PS-to-reconnect keeps working. The command-complete status for that command is ignored once configured, so it doesn't trigger a reset. Commit `971ae6e`.

## Latency: how it was measured and what we know

Debug branch tools:
- **Network logs:** PS2 on Ethernet with its IP set in OPL, and `python tools/ps2_log.py` (UDP 18194) on the PC.
- **`tools/lag_monitor.py`** (debug branch) reads the per-second lines `LAT2`/`LAT4`/`LAT3`.
  - IOP printf truncates long lines, so keep log lines short.
  - The report age is clamped to fix a 4294967 ms wrap.

Results on the test setup (DualSense, ~740 reports/s):
- From when the PS2 receives a press until the game reads it: **~8.2 ms average, ~16.4 ms worst** (one frame). That's just the game polling once per frame; the driver adds under 1 ms.
- **Before v1.1** there were 1.2 pauses per second of ~13 ms. That was two sources together: page scan (every 1.28 s) and something else every 2.56 s.
- **After the page-scan fix**, only the 2.56 s one is left: 0.4 pauses per second of ~14 ms. It's almost certainly the dongle's or the pad's own radio activity. We decided **not to chase it**: changing the link role risks breaking reconnection for ~1 in 180 presses arriving up to one frame late.
- The air latency itself (pad to dongle) isn't measurable from the PS2.
- The "input lag" Rodrigo felt turned out to be his TV, not the driver.

## Lessons and gotchas

- **Test on the monitor, not the TV,** before blaming the driver for lag.
- Some HCI events don't fit in the 64-byte `hci_buf` (Remote Name Complete). The leftover shows as harmless "Unmanaged Event 0x00".
- **PADEMU multitap emulation:**
  - The pad goes to the *first enabled, free* slot. 1A is the same bit as the old 1P, so it's usually still on.
  - With the virtual multitap on port 1, a wired DS2 must go in port 2.
  - Many games number players by order, not by slot.
- **A per-game PADEMU config overrides the global one.**
- **PS + Share/Create inside a game** puts the pad in pairing mode (solid light, no response). In-game, only press PS.

## History

- **v1.0:** DualSense over BT (menu + in-game), SDP server, USB pairing for the DualSense, wireless pairing (SSP) with per-pad keys, `DS3/DS4/DS5 BT` label.
- **v1.1:** rumble output only on change, faster CRC32, page scan off while all pads are connected. Fewer hiccups, same latency otherwise.

## Pending: PR #1 from Miguel (`MiguelQueiroz010:mtap-menu-redesign` → `dualsense-bt`)

58 commits, 44 files, +7.4k/−0.5k lines. Title: "Bluetooth pads in OPL: DS4/clones, Realtek dongles, multi-pad menu, Controller Setup, multitap, KH2 memory fix". Rodrigo already tested Miguel's earlier build (before this PR) with the DualSense, and everything worked: pairing, gameplay, rumble, reconnect, battery indicator.

What it brings:
- **DS4 clones:** legacy PIN pairing ("0000"), DS4 rumble/lightbar with CRC32.
- **Realtek RTL8761B firmware loader** (~26 KB blob from linux-firmware):
  - Only kicks in when the chip runs from ROM.
  - Loaded by the menu module only; in-game it's excluded unless built with `-DPADEMU_RTL_FW`.
  - The firmware stays in the dongle's RAM across the game-launch USB reset (confirmed by Miguel).
- **Battery indicator** in the menu, lightbar colour per slot.
- **Controller Setup** (`include/padsetup.h`, 28-byte blob passed to pademu before the keys): button remap/disable, PS tap = button, PS hold = IGR/power off/screenshot, touchpad modes.
- **Multitap emulation with mixed physical + BT pads**, BT pads 3 and 4 working in the menu, redesigned minimal menu.
- **KH2-over-SMB IOP memory fix:** no IP reassembly/fragmentation in in-game SMSTCPIP, smaller USBD pool, debug builds using the same small lwIP as release.

Review checklist (agreed with Miguel):
1. Rebased on current `dualsense-bt`: `971ae6e` (`bt_update_page_scan`) must still be there and called from both places.
2. No test junk: his old branch had a 70k-line `tools/ps2_log.txt`.
3. Debug/metrics/test UI only in DEBUG builds; the release menu stays clean. Controller Test can stay if it's in a submenu.
4. **Pademu args layout changed** (settings 8 bytes + padsetup 28 + keys). Check `iopmgr.c`, `pademu.c`, `coreconfig.h` and the `_Static_assert` stay consistent.
5. In-game IOP size: compare `bt_pademu.irx` with the v1.1 one. His earlier build was +~3 KB; flag anything much bigger.
6. Builds cleanly (release + debug).
7. Hardware test by Rodrigo:
   - menu boot with the Realtek dongle;
   - wireless pairing from scratch;
   - PES 6 / GoW 2 in-game;
   - buttons, sticks, rumble, lightbar;
   - turn the pad off/on in-game;
   - mixed DS2 on port 2;
   - a long session.
8. Then merge and release **v1.2**, crediting both authors in the README.

Later (separate PRs): menu aesthetics if they're not in #1, Toshiba dongle support (only with hardware to test).

## Ideas backlog (not implemented)

- **Triggers to right stick (Y axis)** for racing games that use the right stick for gas/brake.
  - L2/R2 are already sent as analog pressure; what's missing is the remap to the stick.
  - Check whether Controller Setup from PR #1 already covers part of it.
- **Auto pairing when no pad is detected.** Conditions: no DS2, a dongle present, and no known pad connecting within a few seconds. Then:
  - search;
  - auto-select the first DS4/DS5 found;
  - save the keys;
  - auto-enable PADEMU (BT mode, port 1);
  - show on-screen instructions;
  - cancel on any button press.
- Test 2–4 BT pads at once (only one has been tested).

## Communication

- Rodrigo writes in Spanish (Argentina, voseo). Reply in Spanish unless asked otherwise; repo text (README, commits, release notes) stays in English.
- Messages for Miguel go in Brazilian Portuguese, natural and friendly.
- Reddit: AI use is disclosed (r/ps2homebrew rule). Replies should be short and human-sounding, without heavy formatting. A polished, bullet-heavy answer got called out as "fed to Claude".
- Commit trailer used in this repo:
  ```
  Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
  ```
