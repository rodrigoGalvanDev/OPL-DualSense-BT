#!/usr/bin/env python3
"""Procura o controle "Wireless Controller" no PC e mostra o que o identifica (VID/PID, classe, endereco, fabricante),
para comparar um DS4 original com um clone (como o que o PADEMU precisa reconhecer).

Uso:
    python tools/find_ds4_clone.py            # lista os candidatos e um veredito
    python tools/find_ds4_clone.py --dump     # tambem grava todas as propriedades em ds4_probe.txt
    python tools/find_ds4_clone.py --name "Wireless Controller"   # outro nome

Windows: usa o PowerShell (Get-PnpDevice). Linux: usa o bluetoothctl. Deixe o controle ligado/conectado no PC.
"""
import argparse
import json
import platform
import re
import subprocess
import sys

SONY_VID = 0x054C
SONY_DS4_PIDS = {0x05C4: "DS4 v1 (CUH-ZCT1)", 0x09CC: "DS4 v2 (CUH-ZCT2)", 0x0BA0: "DS4 USB wireless adapter"}
SONY_DUALSENSE_PIDS = {0x0CE6: "DualSense", 0x0DF2: "DualSense Edge"}
BT_VID_SOURCES = {0x0001: "Bluetooth SIG", 0x0002: "USB-IF"}  # "Device ID" profile: de quem e o VID


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace").stdout


def ps_json(script):
    out = run(["powershell", "-NoProfile", "-Command", script + " | ConvertTo-Json -Depth 4 -Compress"]).strip()
    if not out:
        return []
    data = json.loads(out)
    return data if isinstance(data, list) else [data]


def parse_instance_id(iid):
    """Extrai VID/PID/endereco do InstanceId do Windows. Devolve um dict com o que achar."""
    info = {}
    m = re.search(r"VID&([0-9A-F]{4})([0-9A-F]{4})_PID&([0-9A-F]{4})", iid, re.I)  # HID sobre Bluetooth: VID&<fonte><vid>
    if m:
        info["vid_source"] = int(m.group(1), 16)
        info["vid"] = int(m.group(2), 16)
        info["pid"] = int(m.group(3), 16)
    else:
        m = re.search(r"VID_([0-9A-F]{4})&PID_([0-9A-F]{4})", iid, re.I)  # USB
        if m:
            info["vid"] = int(m.group(1), 16)
            info["pid"] = int(m.group(2), 16)
    m = re.search(r"DEV_([0-9A-F]{12})", iid, re.I)
    if m:
        info["bdaddr"] = ":".join(m.group(1)[i:i + 2] for i in range(0, 12, 2)).upper()
    return info


def class_of_device_text(cod):
    """Decodifica a Class of Device (24 bits) em major/minor, como o PADEMU a le na busca."""
    major = (cod >> 8) & 0x1F
    minor = (cod >> 2) & 0x3F
    majors = {1: "Computer", 2: "Phone", 3: "LAN/AP", 4: "Audio/Video", 5: "Peripheral", 6: "Imaging", 7: "Wearable", 8: "Toy", 9: "Health"}
    text = "major=%s(0x%02X) minor=0x%02X" % (majors.get(major, "?"), major, minor)
    if major == 5:
        kind = minor & 0x0F  # bits 5..2 da CoD; os bits 7..6 dizem se e teclado/mouse
        text += " [periferico: %s]" % {0: "nao categorizado", 1: "joystick", 2: "gamepad", 3: "controle remoto", 4: "sensor", 5: "digitalizador", 6: "leitor de cartao"}.get(kind, "outro")
    return text


def verdict(info):
    reasons = []
    vid, pid = info.get("vid"), info.get("pid")
    if vid is None:
        return "indeterminado (sem VID/PID no identificador)", ["o Windows nao expos VID/PID para este item"]
    if vid == SONY_VID and pid in SONY_DS4_PIDS:
        label = "provavel ORIGINAL: VID/PID de %s" % SONY_DS4_PIDS[pid]
    elif vid == SONY_VID and pid in SONY_DUALSENSE_PIDS:
        label = "e um %s original (nao e DS4)" % SONY_DUALSENSE_PIDS[pid]
    elif vid == SONY_VID:
        label = "VID da Sony com PID desconhecido (0x%04X): confira" % pid
    else:
        label = "possivel CLONE: o VID nao e o da Sony"
        reasons.append("VID 0x%04X (Sony = 0x054C)" % vid)
    if info.get("vid_source") not in (None, 0x0002):
        reasons.append("fonte do VID = %s (o DS4 original usa USB-IF)" % BT_VID_SOURCES.get(info["vid_source"], hex(info["vid_source"])))
    return label, reasons


def windows_probe(name, dump_file):
    script = ("Get-PnpDevice | Where-Object { $_.FriendlyName -like '*%s*' } | "
              "Select-Object Status,Class,FriendlyName,InstanceId,Manufacturer,Service" % name.replace("'", "''"))
    devs = ps_json(script)
    results = []
    for d in devs:
        iid = d.get("InstanceId") or ""
        info = parse_instance_id(iid)
        info.update(status=d.get("Status"), cls=d.get("Class"), friendly=d.get("FriendlyName"), manufacturer=d.get("Manufacturer"),
                    service=d.get("Service"), instance=iid)
        props = ps_json("Get-PnpDeviceProperty -InstanceId '%s' | Select-Object KeyName,Data" % iid.replace("'", "''"))
        for p in props:  # a Class of Device aparece como numero em uma propriedade do Bluetooth
            key = (p.get("KeyName") or "")
            if "2BD67D8B-8BEB-48D5-87E0-6CDA3428040A" in key.upper() and isinstance(p.get("Data"), int):
                info.setdefault("cod_candidates", []).append((key, p["Data"]))
        info["props"] = props
        results.append(info)
        if dump_file:
            dump_file.write("== %s\n" % iid)
            for p in props:
                dump_file.write("%s = %s\n" % (p.get("KeyName"), p.get("Data")))
            dump_file.write("\n")
    return results


def linux_probe(name, dump_file):
    results = []
    for line in run(["bluetoothctl", "devices"]).splitlines():
        m = re.match(r"Device\s+([0-9A-F:]{17})\s+(.*)", line, re.I)
        if not m or name.lower() not in m.group(2).lower():
            continue
        text = run(["bluetoothctl", "info", m.group(1)])
        info = {"bdaddr": m.group(1).upper(), "friendly": m.group(2), "instance": m.group(1), "props": []}
        mm = re.search(r"Modalias:\s*(\w+):v([0-9A-F]{4})p([0-9A-F]{4})", text, re.I)
        if mm:
            info["vid"], info["pid"] = int(mm.group(2), 16), int(mm.group(3), 16)
            info["vid_source"] = 0x0002 if mm.group(1).lower() == "usb" else 0x0001
        mc = re.search(r"Class:\s*0x([0-9A-F]+)", text, re.I)
        if mc:
            info["cod_candidates"] = [("Class", int(mc.group(1), 16))]
        if dump_file:
            dump_file.write("== %s\n%s\n" % (m.group(1), text))
        results.append(info)
    return results


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--name", default="Wireless Controller", help="nome do dispositivo (padrao: Wireless Controller)")
    ap.add_argument("--dump", action="store_true", help="grava todas as propriedades em ds4_probe.txt")
    args = ap.parse_args()

    dump_file = open("ds4_probe.txt", "w", encoding="utf-8") if args.dump else None
    system = platform.system()
    if system == "Windows":
        results = windows_probe(args.name, dump_file)
    elif system == "Linux":
        results = linux_probe(args.name, dump_file)
    else:
        sys.exit("Sistema nao suportado: use Windows ou Linux.")
    if dump_file:
        dump_file.close()

    if not results:
        print('Nenhum dispositivo com "%s" encontrado. Ligue o controle (PS + Share para parear) e tente de novo.' % args.name)
        return

    for r in results:
        label, reasons = verdict(r)
        print("-" * 70)
        print("Nome       : %s" % r.get("friendly"))
        print("Identificador: %s" % r.get("instance"))
        if r.get("status"):
            print("Estado     : %s   Classe do Windows: %s   Servico/driver: %s" % (r.get("status"), r.get("cls"), r.get("service")))
        if r.get("manufacturer"):
            print("Fabricante : %s" % r["manufacturer"])
        if "vid" in r:
            print("VID:PID    : %04X:%04X   (fonte do VID: %s)" % (r["vid"], r["pid"], BT_VID_SOURCES.get(r.get("vid_source"), "USB direto" if "vid_source" not in r else hex(r["vid_source"]))))
        if "bdaddr" in r:
            print("Endereco BT: %s   (OUI = %s)" % (r["bdaddr"], r["bdaddr"][:8]))
        for key, cod in r.get("cod_candidates", []):
            print("Class of Device: 0x%06X  -> %s   [%s]" % (cod & 0xFFFFFF, class_of_device_text(cod), key.split()[-1] if key else "?"))
        print("Veredito   : %s" % label)
        for why in reasons:
            print("   - %s" % why)
    print("-" * 70)
    print("Dica: rode com um DS4 original e com o clone e compare VID:PID, OUI e Class of Device.")
    print("O PADEMU aceita gamepad/joystick (periferico) e, depois do patch de clones, tambem outras classes.")
    if args.dump:
        print("Propriedades completas gravadas em ds4_probe.txt")


if __name__ == "__main__":
    main()
