# OPL-DualSense-BT

A fork of [Open PS2 Loader](https://github.com/ps2homebrew/Open-PS2-Loader) that lets you play your PS2 with a **DualSense or DualShock 4 over Bluetooth**. No cable from the couch to the console.

## What's new

- **DualSense over Bluetooth**, both in the OPL menu and in-game. Buttons, sticks, rumble and lightbar all work.
- **Wireless pairing from the OPL menu.** `Bluetooth Pads → Search`: put the pad in pairing mode, pick it from the list, done. No USB cable, no PC.
- **USB pairing for the DualSense** too, with the same `Pair` button OPL already had for the DS3/DS4.
- **DualShock 4 clones:** old PIN pairing, and rumble/lightbar that clones accept.
- **Realtek RTL8761B/BU dongles:** the menu loads their firmware by itself when the dongle runs from ROM.
- **Up to 4 Bluetooth pads**, all of them can move around the OPL menu, with a battery indicator per pad.
- **Controller Setup:** button remapping, what the PS button does, touchpad modes (DS4) and the lightbar colour of each pad.
- **Multitap emulation** that mixes Bluetooth pads with a real controller plugged in the port.
- **More free IOP memory over SMB** with PADEMU on (Kingdom Hearts II needed it).

Under the hood, the DualSense needed three things stock OPL doesn't have:
- its own input/output reports, including the CRC32 that output reports need;
- a tiny SDP server, because the pad asks the console what services it has and drops the link after ~30 s if nobody answers;
- Secure Simple Pairing for the wireless search, with a link key stored per pad.

## Getting started

Grab the `.ELF` from the [Releases](../../releases) page and launch it however you usually launch OPL (FreeMcBoot, uLaunchELF, a modchip, whatever). It's a regular OPL, so your games and settings work as usual.

You need a **USB Bluetooth dongle** plugged into the PS2 (see [Compatibility](#compatibility)).

### 1. Turn on the pad emulator

- **For all games:** `Start → Controller Settings → Configure PADEMU`
- **For a single game:** select the game, press `Triangle`, then go to `Configure PADEMU`

Then set:
- `Pad Emulator` → **On**
- `Pad Emulator Mode` → **DS3/DS4/DS5 BT**
- On the row of each port (or multitap slot), pick **Emulated**, or **Emulated + Vibration** if you want rumble. **Real controller** leaves that port to the pad plugged into it.

<img src="docs/img/pademu-settings.jpg" alt="Pad Emulator Settings with the DS3/DS4/DS5 BT mode" width="640">

Don't forget `Save Changes`. If you skip it, the game launches without the emulator and you'll swear the pad is broken (been there).

### 2. Pair the pad (only once)

**Wireless (recommended):**
1. In the same PADEMU screen, open `Bluetooth Pads` and press `Search`.
2. Hold **PS + Create** on the DualSense until the light blinks fast (DS4: **PS + Share**).
3. Your pad shows up in the list. Select it and wait for `Paired`, then press PS.

**Note:** Wireless pairing is for the DualSense and the DS4 (an original DS4 hasn't been confirmed yet). The DS3 has no pairing mode, so it still needs the cable method below.

<img src="docs/img/wireless-pairing.jpg" alt="Wireless Pairing screen finding a DualSense" width="640">

**With a cable:**
1. Plug the pad into the PS2 with a USB cable (dongle in the other port).
2. In `Bluetooth Pads`, press `Pair`, unplug the cable and press PS.

The pairing is saved, so you don't have to do it again. Next time just press PS.

### 3. Play

Launch the game and press **PS** on the pad once the game is on screen. Launching a game resets the PS2's USB side, so the pad always disconnects for a second and needs that PS press to come back. Reconnecting takes a fraction of a second.

**Holding PS** for 2 seconds in-game turns the pad off, like on a PS4/PS5. Press PS again to reconnect it. You can change that in Controller Setup.

## Controller Setup

`Configure PADEMU → Controller Setup` (Bluetooth mode only). These settings are global, not per game.

- **PS button, short press:** press a PS2 button of your choice (default: nothing).
- **PS button, hold:** turn off the controller (default), exit to OPL (IGR), power off the PS2, take a screenshot (needs GSM), or nothing. Hold time: 1, 2 or 3 seconds.
- **Touchpad (DS4 only):** Select/Start halves (default), click as a button, or drive the left/right stick by dragging or like a mouse.
- **Lightbar:** a colour per slot, or a custom one with a colour picker. On a pad paired wirelessly the colour is saved with the pad, so it keeps it whatever slot it lands in.
- **Button remapping:** send each button as another PS2 button, or disable it.

## Compatibility

Honestly, it's only been tested on a couple of setups so far. Everything else comes from how the code works, not from real testing. If you try it, please report back (see below).

### Tested

| What | Model |
|---|---|
| Console | PS2 Slim SCPH-90001 (Matrix modchip) |
| Dongle | Realtek, USB ID `0BDA:8771` |
| Controller | DualSense (wireless pairing, USB pairing, menu and in-game) |
| Mixed | DualSense on 1P via Bluetooth + wired DualShock 2 on 2P |

### Should work

- **Any PS2 that runs OPL** (fat or slim). There's nothing model-specific in here.
- **Dongles:** most USB Bluetooth adapters that don't need firmware loaded by the OS, plus **Realtek RTL8761B/BU** (the menu loads its firmware). CSR8510-based ones are the classic pick for PADEMU. The wireless search needs **Bluetooth 2.1 or newer**. USB pairing works with older dongles too.
- **Original DualShock 4** (both CUH-ZCT1 and ZCT2), and DS4 clones that answer the wireless search.
- **DualSense Edge:** same reports as the regular one and it's recognized by name, but nobody has tried it yet.

### Probably won't work / unknown

- **Dongles that need other firmware uploaded** (some Intel and Broadcom ones) and **BLE-only dongles**.
- **Cheap clone CSR dongles:** a coin toss.
- **Some fake DS4s:** depends on the clone.

### Limitations

- Pads 3 and 4 move around the OPL menu, but in-game they need the multitap emulation turned on.
- Up to **4 pads** paired with the wireless search are remembered. Pairing a 5th forgets the oldest one.
- Only one Bluetooth pad at a time has been tested in-game so far.
- Some games just don't like PADEMU (games with their own pad drivers, or that use USB for something else). That's the same as stock OPL.
- The pairing format changed in this version (it now stores the lightbar colour). If you go back to an older build, pair your pads again.

### Report your setup

Open an [issue](../../issues) with:
- PS2 model (e.g. SCPH-90001)
- Dongle (brand, and the USB ID if you can get it: `xxxx:xxxx`)
- Controller (DualSense, Edge, DS4 original/clone...)
- What worked: USB pairing / wireless search / menu / in-game

If something's broken, a log helps a ton (see the next section).

## Debug build and logs

The debug build sends logs over the network. To use it:

1. Connect the PS2 to your router with an Ethernet cable and set its IP in OPL's network settings.
2. On your PC (same network), run `python tools/ps2_log.py`. It listens on UDP port 18194 and saves everything to `ps2_log.txt`.
3. Launch the debug ELF, reproduce the problem and attach `ps2_log.txt` to your issue.

## Building

Same as regular OPL: you need [ps2dev/ps2sdk](https://github.com/ps2dev/ps2dev) set up (`PS2DEV` and `PS2SDK` exported), then:

```sh
make download_lng
make            # release
make DEBUG=1 INGAME_DEBUG=1   # debug build with network logs
```

After changing shared headers (such as `ee_core/include/coreconfig.h`), do a `make clean` once.

See [README-OPL.md](README-OPL.md) for the full upstream docs (game formats, devices, settings and so on).

### Where the changes live

- `modules/ds34bt/iop/ds34bt.c`: Bluetooth driver for the OPL menu (DualSense, SDP, wireless search and pairing, Realtek firmware loader)
- `modules/pademu/ds34bt.c`: in-game Bluetooth driver (Controller Setup is applied here)
- `modules/pademu/pademu.c`, `pademu_route.h`, `pademu_split.h`: which pad answers each port and multitap slot
- `modules/ds34usb/iop/ds34usb.c`: USB pairing for the DualSense
- `include/btkeys.h`: per-pad link keys for wireless pairing
- `include/padsetup.h`: Controller Setup settings
- `src/guigame.c`, `src/dialogs.c`: the PADEMU, Bluetooth Pads and Controller Setup screens

## Credits

- **Rodrigo Galvan** ([@rodrigoGalvanDev](https://github.com/rodrigoGalvanDev)): DualSense over Bluetooth, SDP server, wireless pairing, latency fixes.
- **Miguel Queiroz** ([@MiguelQueiroz010](https://github.com/MiguelQueiroz010)): DS4 clones, Realtek firmware loader, multi-pad menu and battery indicator, Controller Setup, multitap with real + Bluetooth pads, IOP memory fix.
- The [Open PS2 Loader](https://github.com/ps2homebrew/Open-PS2-Loader) team and everyone behind PADEMU. This is 99% their work.
- The Bluetooth driver is based on [Arduino-PSRemote](https://github.com/IonAgorria/Arduino-PSRemote) and the [USB Host Shield 2.0](https://github.com/felis/USB_Host_Shield_2.0) library, same as upstream.
- Realtek firmware from [linux-firmware](https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git).

## License

Same as OPL: [Academic Free License v3.0](LICENSE).
