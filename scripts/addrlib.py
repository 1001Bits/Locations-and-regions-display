"""Parse F4SE address library .bin files and cross-reference offsets between
runtime versions.

Format (per CommonLibF4 src/REL/IDDB.cpp):
    uint64 count
    repeated { uint64 id; uint64 offset; }[count]   (sorted by id)
"""

import struct
import sys
from pathlib import Path

OG = Path(r"C:\Games\Fallout.4 1.10.163\Data\F4SE\Plugins\version-1-10-163-0.bin")
NG_984 = Path(r"C:\Games\Steam\steamapps\common\Fallout 4 AE\Data\F4SE\Plugins\version-1-10-984-0.bin")
NG_191 = Path(r"C:\Games\Steam\steamapps\common\Fallout 4 AE\Data\F4SE\Plugins\version-1-11-191-0.bin")


def load(path):
    data = path.read_bytes()
    (count,) = struct.unpack_from("<Q", data, 0)
    entries = struct.iter_unpack("<QQ", data[8 : 8 + count * 16])
    id2off = {}
    off2id = {}
    for id_, off in entries:
        id2off[id_] = off
        off2id[off] = id_
    return id2off, off2id


def main():
    og_id2off, og_off2id = load(OG)
    ng984_id2off, ng984_off2id = load(NG_984)
    ng191_id2off, ng191_off2id = load(NG_191)

    print(f"OG entries:     {len(og_id2off):>10,}")
    print(f"NG 1.10.984:    {len(ng984_id2off):>10,}")
    print(f"NG 1.11.191:    {len(ng191_id2off):>10,}")
    print()

    # NG-1.10.984 offsets we found via Ghidra (addresses in Fallout4.exe.unpacked.exe).
    # Cross-reference against 1.11.191 using the bin's NG-side ID scheme.
    ng_to_1_11_191(ng984_id2off, ng191_id2off)

    print("\n=== UpdateLocationVignette / dispatcher: OG <-> NG 1.10.984 <-> NG 1.11.191 ===")
    # OG offsets (1.10.163, RVAs):
    og_targets = [
        ("UpdateLocationVignette",  0xa66380),
        ("HUDNotificationData ctor (HDT_LOC ver)", 0xa65000),  # guess; verify
    ]
    # Map OG offset -> OG ID -> NG 1.10.984 offset (via shared ID, but OG and NG
    # have DIFFERENT ID schemes for many functions). So instead we go OG->NG via
    # known-good points or symbol search.
    for name, og_off in og_targets:
        og_id = og_off2id.get(og_off)
        print(f"{name:<45} OG=0x{og_off:08X}  OG ID={og_id}")

    # NG 1.11.191 known offsets we just discovered via binary analysis:
    ng191_targets = [
        ("UpdateLocationVignette (AE 1.11.191)", 0xa37560),
        ("Notification dispatcher",              0xa39b50),
        ("HUDNotificationData ctor",             0x9ea220),
        ("Notification queue (called by disp.)", 0xa38a10),
    ]
    for name, ng191_off in ng191_targets:
        id_ = ng191_off2id.get(ng191_off)
        if id_ is None:
            print(f"{name:<45} NG1.11.191=0x{ng191_off:08X}  NO ID")
        else:
            ng984_off = ng984_id2off.get(id_)
            ng984_s = f"0x{ng984_off:08X}" if ng984_off else "-"
            print(f"{name:<45} NG1.11.191=0x{ng191_off:08X}  ID={id_:<8}  NG1.10.984={ng984_s}")


def ng_to_1_11_191(ng984_id2off, ng191_id2off):
    print("\n=== NG 1.10.984 -> 1.11.191 lookup ===")
    ng984_off2id = {v: k for k, v in ng984_id2off.items()}
    ng_targets = [
        ("BSTValueEventSource<HUDND>::SetValue", 0x9A09E0),
        ("HUDDataModel singleton ptr", 0x2FF2078),
        ("HUDNotificationsModel::UpdateLocationVignette", 0x9985B0),
        ("HUDNotificationsModel::HUDNotificationsModel ctor", 0x996AE0),
        ("HUDDataModel constructor", 0x98BE40),
        ("BSTValueEventSource<HUDND> dtor (FUN_14099b380)", 0x99B380),
        ("FUN_14099ca30 (notify-only inner helper)", 0x99CA30),
    ]
    for name, ng984_off in ng_targets:
        id_ = ng984_off2id.get(ng984_off)
        if id_ is None:
            print(f"{name:<55} NG984=0x{ng984_off:08X}  NO ID")
            continue
        ng191 = ng191_id2off.get(id_)
        ng191_s = hex(ng191) if ng191 is not None else "-"
        print(
            f"{name:<55} NG984=0x{ng984_off:08X}  ID={id_:<8}  NG1.11.191={ng191_s}"
        )


if __name__ == "__main__":
    main()
