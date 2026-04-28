"""Trace the 6 callers of SetValue in 1.11.191 to find the location-vignette
caller — the engine's analogue of OG's HUDNotificationsModel::UpdateLocationVignette
(0x140a66380 on OG). That's the function that broadcasts the location title
on first discovery, which we want to mimic exactly.

Strategy: dump 32 instructions before each SetValue call. The location-vignette
caller will reference a specific BSFixedString interned for "HDT_LOCATION_DISCOVERED",
or build a HUDNotificationDisplayEvent on the stack with both title + subtitle
fields populated. Other SetValue callers are siblings (HUDLocationText, vault
transitions, achievement banners, etc).
"""

import pefile
import struct
from pathlib import Path
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

BIN = Path(r"C:\Games\Steam\steamapps\common\Fallout 4 AE\Fallout4.exe.unpacked.exe")
IMAGE_BASE = 0x140000000
SETVALUE_RVA = 0x9F41B0
SINGLETON_RVA = 0x32694E8


def disasm_window(rva, before=80, after=8, label=""):
    pe = pefile.PE(str(BIN), fast_load=True)
    file_off = pe.get_offset_from_rva(rva - before)
    with open(BIN, "rb") as f:
        f.seek(file_off)
        bytes_ = f.read(before + after + 16)

    md = Cs(CS_ARCH_X86, CS_MODE_64)
    print(f"\n=== {label} (window before+at+after RVA 0x{rva:x}) ===")
    for ins in md.disasm(bytes_, IMAGE_BASE + (rva - before)):
        marker = " <-- CALL" if ins.address == IMAGE_BASE + rva else ""
        print(f"  0x{ins.address:x}: {ins.mnemonic:<8} {ins.op_str}{marker}")
        if ins.address > IMAGE_BASE + rva + after:
            break


def find_callers(target_rva):
    pe = pefile.PE(str(BIN), fast_load=True)
    text = next(s for s in pe.sections if s.Name.rstrip(b"\x00") == b".text")
    file_off = text.PointerToRawData
    with open(BIN, "rb") as f:
        f.seek(file_off)
        bytes_ = f.read(text.SizeOfRawData)

    callers = []
    for i in range(len(bytes_) - 5):
        if bytes_[i] == 0xE8:
            disp = struct.unpack_from("<i", bytes_, i + 1)[0]
            next_rip = IMAGE_BASE + text.VirtualAddress + i + 5
            if next_rip + disp == IMAGE_BASE + target_rva:
                callers.append(text.VirtualAddress + i)
    return callers


def check_singleton_init(singleton_rva):
    """Read the value stored in .data at singleton_rva — is it zero
    (heap-allocated at runtime) or pre-populated (static init)?"""
    pe = pefile.PE(str(BIN), fast_load=True)
    file_off = pe.get_offset_from_rva(singleton_rva)
    with open(BIN, "rb") as f:
        f.seek(file_off)
        raw = f.read(8)
    val = struct.unpack("<Q", raw)[0]
    print(f"\n=== Singleton slot at RVA 0x{singleton_rva:x} (in PE file) ===")
    print(f"  Raw 8 bytes: {raw.hex()}")
    print(f"  As uint64:   0x{val:x}")
    print(f"  Note: zero in file => heap-init at runtime; non-zero => static")


def main():
    check_singleton_init(SINGLETON_RVA)

    callers = find_callers(SETVALUE_RVA)
    print(f"\nFound {len(callers)} SetValue callers")
    for i, c in enumerate(callers):
        disasm_window(c, before=120, after=4, label=f"Caller #{i} @ 0x{c:x}")


if __name__ == "__main__":
    main()
