"""Decode the first MOV instruction of the 1.11.191 HUDDataModel constructor to
recover the RIP-relative address it stores into — that's the HUDDataModel
singleton pointer location.

Address library cross-ref already gave us the constructor's RVA on 1.11.191
(0x9df5c0 = 0x1409df5c0 with the standard 0x140000000 image base). The first
instruction matches the 1.10.984 pattern:

    DAT_<singleton> = (undefined8 *)0x0;

which compiles to:

    48 c7 05 <imm32 disp> 00 00 00 00      ; mov qword [rip+disp], 0

Decoding `disp` (signed 32-bit, RIP-relative from the next instruction) gives
the singleton address.
"""

import pefile
import struct
from pathlib import Path

BIN = Path(r"C:\Games\Steam\steamapps\common\Fallout 4 AE\Fallout4.exe.unpacked.exe")
CTOR_RVA = 0x9DF5C0  # HUDDataModel constructor on 1.11.191
IMAGE_BASE = 0x140000000


def main():
    pe = pefile.PE(str(BIN), fast_load=True)
    file_off = pe.get_offset_from_rva(CTOR_RVA)
    print(f"PE image base: 0x{pe.OPTIONAL_HEADER.ImageBase:x}")
    print(f"Constructor RVA: 0x{CTOR_RVA:x}, file offset: 0x{file_off:x}")

    with open(BIN, "rb") as f:
        f.seek(file_off)
        bytes_ = f.read(2048)

    # Trace `lea rcx, [rdi+disp32]` followed by a `call rel32` — that's the
    # standard MSVC pattern for invoking a sub-object constructor at a fixed
    # offset within `this`. Match each call's target against known sub-object
    # ctor addresses to identify the layout.
    HNM_CTOR_191 = 0x9EA2B0
    print("\nlea+call pairs in HUDDataModel ctor:")
    i = 0
    while i < len(bytes_) - 12:
        # 48 8d 8f <disp32>  ; lea rcx, [rdi+disp32]
        if bytes_[i : i + 3] == b"\x48\x8d\x8f":
            sub_off = struct.unpack_from("<i", bytes_, i + 3)[0]
            j = i + 7
            if j + 5 <= len(bytes_) and bytes_[j] == 0xE8:
                call_disp = struct.unpack_from("<i", bytes_, j + 1)[0]
                next_rip = CTOR_RVA + j + 5
                target = next_rip + call_disp
                marker = " <-- HUDNotificationsModel ctor!" if target == HNM_CTOR_191 else ""
                print(f"  +0x{i:03x}: lea rcx, [rdi+0x{sub_off & 0xffffffff:x}]; call 0x{target:x}{marker}")
                i = j + 5
                continue
        i += 1

    # Scan for `48 c7 05 <disp32> 00 00 00 00` (mov qword [rip+disp32], 0)
    # OR for `48 89 0d <disp32>` (mov qword [rip+disp32], rcx) — used by the
    # second store that puts the actual `param_1` into the singleton slot.
    print("\nMOV-to-singleton candidates:")
    for i in range(len(bytes_) - 11):
        if bytes_[i : i + 3] == b"\x48\xc7\x05" and bytes_[i + 7 : i + 11] == b"\x00\x00\x00\x00":
            disp = struct.unpack_from("<i", bytes_, i + 3)[0]
            next_rip = CTOR_RVA + i + 11
            target_rva = next_rip + disp
            print(
                f"  +0x{i:03x}: mov qword [rip+0x{disp:08x}], 0       "
                f"-> singleton RVA 0x{target_rva:x}  (runtime 0x{IMAGE_BASE + target_rva:x})"
            )
        # 48 89 ?? = REX.W mov [rip+disp32], reg — second byte ModR/M:
        #   0d = rcx, 15 = rdx, 1d = rbx, 35 = rsi, 3d = rdi, 25 = rsp, 2d = rbp, 05 = rax
        if bytes_[i : i + 2] == b"\x48\x89" and bytes_[i + 2] in (
            0x05, 0x0D, 0x15, 0x1D, 0x25, 0x2D, 0x35, 0x3D,
        ):
            modrm = bytes_[i + 2]
            reg = {0x05: "rax", 0x0D: "rcx", 0x15: "rdx", 0x1D: "rbx",
                   0x25: "rsp", 0x2D: "rbp", 0x35: "rsi", 0x3D: "rdi"}[modrm]
            disp = struct.unpack_from("<i", bytes_, i + 3)[0]
            next_rip = CTOR_RVA + i + 7
            target_rva = next_rip + disp
            print(
                f"  +0x{i:03x}: mov [rip+0x{disp & 0xffffffff:08x}], {reg}    "
                f"-> RVA 0x{target_rva:x}  (runtime 0x{IMAGE_BASE + target_rva:x})"
            )


if __name__ == "__main__":
    main()
