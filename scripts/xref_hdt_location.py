"""Find code references to the 'HDT_LOCATION_DISCOVERED' string in 1.11.191.
That string is interned at a fixed RVA — code that loads it via lea rcx,[rip+disp]
identifies the location-vignette code path. From there we can xref backwards
to find the actual UpdateLocationVignette equivalent on AE.

Pattern: `48 8d 0d <disp32>` (lea rcx, [rip+disp]) where the target equals the
string's RVA. Strings can also be loaded into other regs (rdx/r8); cover those
common modr/m bytes.
"""

import pefile
import struct
from pathlib import Path

BIN = Path(r"C:\Games\Steam\steamapps\common\Fallout 4 AE\Fallout4.exe.unpacked.exe")
IMAGE_BASE = 0x140000000
STR_FILE_OFFSET = 0x2524E78  # 'HDT_LOCATION_DISCOVERED'
STR2_FILE_OFFSET = 0x2524E90  # 'HDT_SHOW_LOCATION'


def file_offset_to_rva(file_off):
    pe = pefile.PE(str(BIN), fast_load=True)
    for sec in pe.sections:
        if sec.PointerToRawData <= file_off < sec.PointerToRawData + sec.SizeOfRawData:
            return sec.VirtualAddress + (file_off - sec.PointerToRawData)
    return None


def find_lea_to(target_rva):
    """Find `lea reg, [rip+disp]` instructions that resolve to target_rva,
    where reg can be any of rax/rcx/rdx/rbx/rsi/rdi/rbp/rsp/r8-r15.
    Returns list of (rva, modrm_byte, instruction_length=7)."""
    pe = pefile.PE(str(BIN), fast_load=True)
    text = next(s for s in pe.sections if s.Name.rstrip(b"\x00") == b".text")
    file_off = text.PointerToRawData
    with open(BIN, "rb") as f:
        f.seek(file_off)
        bytes_ = f.read(text.SizeOfRawData)

    # 48 8d <modrm> <disp32>  — modrm with mod=00 r/m=101 selects RIP-relative
    # ModR/M byte: bits 7-6 mod=00, bits 5-3 reg (0-7), bits 2-0 r/m=101
    # So modrm = (reg << 3) | 0x05, where reg is the *low* 3 bits of dest reg
    # REX.W=0x48 (no R bit) covers rax-rdi; REX.WR=0x4c covers r8-r15
    refs = []
    for i in range(len(bytes_) - 7):
        b0, b1, b2 = bytes_[i], bytes_[i+1], bytes_[i+2]
        if (b0 in (0x48, 0x4c)) and b1 == 0x8d and (b2 & 0xC7) == 0x05:
            disp = struct.unpack_from("<i", bytes_, i+3)[0]
            next_rip = text.VirtualAddress + i + 7
            target = next_rip + disp
            if target == target_rva:
                reg_idx = (b2 >> 3) & 0x7
                if b0 == 0x4c:
                    reg_idx += 8
                regs = ["rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
                        "r8","r9","r10","r11","r12","r13","r14","r15"]
                rva = text.VirtualAddress + i
                refs.append((rva, regs[reg_idx]))
    return refs


def find_enclosing_function_start(rva):
    """Walk backwards from rva to find a function-prologue pattern. Bethesda
    uses MSVC, common prologue: int3-padded boundary then `mov [rsp+X], reg`
    or `push rbp/rbx/...` or `sub rsp, ...`. Cheap heuristic: search for the
    nearest preceding `cc cc cc cc` (int3 padding) — function starts after."""
    pe = pefile.PE(str(BIN), fast_load=True)
    text = next(s for s in pe.sections if s.Name.rstrip(b"\x00") == b".text")
    file_off = text.PointerToRawData
    with open(BIN, "rb") as f:
        f.seek(file_off)
        bytes_ = f.read(text.SizeOfRawData)

    # Walk backward from rva looking for 4+ consecutive int3 (0xCC) bytes
    pos = rva - text.VirtualAddress
    for i in range(pos, max(0, pos - 4096), -1):
        if bytes_[i:i+4] == b"\xcc\xcc\xcc\xcc":
            # Find first non-cc byte after this padding
            j = i
            while j < len(bytes_) and bytes_[j] == 0xCC:
                j += 1
            return text.VirtualAddress + j
    return None


def main():
    rva1 = file_offset_to_rva(STR_FILE_OFFSET)
    rva2 = file_offset_to_rva(STR2_FILE_OFFSET)
    print(f"'HDT_LOCATION_DISCOVERED' string at file 0x{STR_FILE_OFFSET:x} -> RVA 0x{rva1:x} (runtime 0x{IMAGE_BASE+rva1:x})")
    print(f"'HDT_SHOW_LOCATION'        string at file 0x{STR2_FILE_OFFSET:x} -> RVA 0x{rva2:x} (runtime 0x{IMAGE_BASE+rva2:x})")

    # However: the engine usually doesn't load the string directly. It loads a
    # *BSFixedString slot* whose .data points to the string. So we should
    # search for xrefs to the slot, not the string itself. The interned slot
    # is typically populated lazily, but the slot's *address* is referenced in
    # code as `lea rcx, [rip+slot_addr]`.
    #
    # First scan the binary for any reference to the string address (these
    # are usually only from the .data section — initialization tables).
    pe = pefile.PE(str(BIN), fast_load=True)
    target_run = IMAGE_BASE + rva1
    print("\nScanning for 8-byte runtime references to the string itself...")
    with open(BIN, "rb") as f:
        raw = f.read()
    refs_data = []
    for i in range(0, len(raw) - 8, 8):
        v = struct.unpack_from("<Q", raw, i)[0]
        if v == target_run:
            refs_data.append(i)
    for r in refs_data[:20]:
        # Convert file offset back to RVA
        for sec in pe.sections:
            if sec.PointerToRawData <= r < sec.PointerToRawData + sec.SizeOfRawData:
                rva = sec.VirtualAddress + (r - sec.PointerToRawData)
                print(f"  file 0x{r:x} -> RVA 0x{rva:x} in section {sec.Name.decode().rstrip(chr(0))}")
                break

    # If that surfaced a .data slot, scan for `lea reg, [rip+disp]` to that slot.
    print("\nlea xrefs to the string itself:")
    for r in find_lea_to(rva1):
        print(f"  RVA 0x{r[0]:x} (runtime 0x{IMAGE_BASE+r[0]:x})  reg={r[1]}")

    # Try to find functions that reference any of the slots we found.
    if refs_data:
        for r in refs_data:
            for sec in pe.sections:
                if sec.PointerToRawData <= r < sec.PointerToRawData + sec.SizeOfRawData:
                    slot_rva = sec.VirtualAddress + (r - sec.PointerToRawData)
                    print(f"\nlea xrefs to slot at RVA 0x{slot_rva:x}:")
                    refs = find_lea_to(slot_rva)
                    for (rva, reg) in refs[:10]:
                        func = find_enclosing_function_start(rva)
                        print(f"  RVA 0x{rva:x} reg={reg}  enclosing func ~0x{func:x}" if func else
                              f"  RVA 0x{rva:x} reg={reg}")
                    break


if __name__ == "__main__":
    main()
