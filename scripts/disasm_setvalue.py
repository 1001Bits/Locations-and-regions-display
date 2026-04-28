"""Disassemble HUDNotificationsModel-related functions on 1.11.191 to verify
our SetValue path matches what the engine itself does for first-discovery
location vignettes.

Two purposes:
  1. Confirm SetValue at 0x9f41b0 actually writes to the same struct slot we
     think (offset 0 = subtitle, offset 8 = title in HUDNotificationDisplayEvent).
  2. Find UpdateLocationVignette's NG equivalent and see how the engine itself
     calls SetValue — that tells us the exact eventSource address and the
     correct event payload layout for 1.11.191.

The OG version (1.10.163) calls UpdateLocationVignette at 0x140a66380, which
constructs a HUDNotificationDisplayEvent on the stack with title/subtitle
strings then calls SetValue(eventSource, &ev). We want to find the NG site
that does the same thing.
"""

import pefile
import struct
from pathlib import Path
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

BIN = Path(r"C:\Games\Steam\steamapps\common\Fallout 4 AE\Fallout4.exe.unpacked.exe")
IMAGE_BASE = 0x140000000

# Functions of interest
SETVALUE_RVA = 0x9F41B0  # ::SetValue (BSTValueEventSource<HUDNotificationDisplayEvent>::SetValue)
HNM_CTOR_RVA = 0x9EA2B0  # HUDNotificationsModel constructor
SINGLETON_RVA = 0x32694E8  # HUDDataModel singleton slot
HNM_OFFSET = 0xC40        # HUDNotificationsModel within HUDDataModel
EV_SOURCE_OFFSET = 0x120  # event source within HNM


def load_bytes(rva, n=512):
    pe = pefile.PE(str(BIN), fast_load=True)
    file_off = pe.get_offset_from_rva(rva)
    with open(BIN, "rb") as f:
        f.seek(file_off)
        return f.read(n), pe.OPTIONAL_HEADER.ImageBase


def disasm(rva, n=512, label="<func>"):
    bytes_, _ = load_bytes(rva, n)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = False
    print(f"\n=== {label} @ RVA 0x{rva:x} ===")
    for ins in md.disasm(bytes_, IMAGE_BASE + rva):
        print(f"  0x{ins.address:x}: {ins.mnemonic:<8} {ins.op_str}")
        # stop at first ret-like instruction or after enough output
        if ins.mnemonic == "ret":
            break


def find_callers(target_rva):
    """Scan .text for `e8 <disp32>` where (next_rip + disp) == target_rva."""
    pe = pefile.PE(str(BIN), fast_load=True)
    text = next(s for s in pe.sections if s.Name.rstrip(b"\x00") == b".text")
    text_rva = text.VirtualAddress
    text_size = text.SizeOfRawData
    file_off = text.PointerToRawData
    with open(BIN, "rb") as f:
        f.seek(file_off)
        bytes_ = f.read(text_size)

    callers = []
    target_runtime = IMAGE_BASE + target_rva
    for i in range(len(bytes_) - 5):
        if bytes_[i] == 0xE8:
            disp = struct.unpack_from("<i", bytes_, i + 1)[0]
            next_rip = IMAGE_BASE + text_rva + i + 5
            if next_rip + disp == target_runtime:
                callers.append(text_rva + i)
    return callers


def find_xrefs_to_address(target_rva):
    """Scan .text for `lea reg, [rip+disp]` or `mov reg, [rip+disp]`
    that resolves to target_rva."""
    pe = pefile.PE(str(BIN), fast_load=True)
    text = next(s for s in pe.sections if s.Name.rstrip(b"\x00") == b".text")
    text_rva = text.VirtualAddress
    text_size = text.SizeOfRawData
    file_off = text.PointerToRawData
    with open(BIN, "rb") as f:
        f.seek(file_off)
        bytes_ = f.read(text_size)

    target_runtime = IMAGE_BASE + target_rva
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True

    refs = []
    for ins in md.disasm(bytes_, IMAGE_BASE + text_rva):
        for op in ins.operands:
            if op.type == 3:  # X86_OP_MEM
                if op.mem.base == 41 and op.mem.disp:  # X86_REG_RIP=41
                    target = ins.address + ins.size + op.mem.disp
                    if target == target_runtime:
                        refs.append((ins.address - IMAGE_BASE, f"{ins.mnemonic} {ins.op_str}"))
    return refs


def main():
    # 1) SetValue itself — verify it writes to BSTValueEventSource.value
    disasm(SETVALUE_RVA, 384, "SetValue (1.11.191 @ 0x9f41b0)")

    # 2) HNM constructor — verify event source offset 0x120
    disasm(HNM_CTOR_RVA, 256, "HUDNotificationsModel ctor (1.11.191 @ 0x9ea2b0)")

    # 3) Find callers of SetValue — these are functions that broadcast events,
    #    one of which should be the equivalent of UpdateLocationVignette.
    callers = find_callers(SETVALUE_RVA)
    print(f"\n=== Callers of SetValue (0x{SETVALUE_RVA:x}): {len(callers)} call sites ===")
    for c in callers[:30]:
        print(f"  0x{c:x}  (runtime 0x{IMAGE_BASE+c:x})")


if __name__ == "__main__":
    main()
