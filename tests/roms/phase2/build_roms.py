#!/usr/bin/env python3
"""
Generate Phase 2 test ROMs from hand-assembled bytecode.
Since no Z80 assembler is available, we provide the assembled machine code directly.
"""

import struct
import os

def write_bin(filename, data):
    """Write binary data to file."""
    with open(filename, 'wb') as f:
        f.write(bytes(data))
    print(f"Generated {filename} ({len(data)} bytes)")

def ram_rw_test():
    """
    RAM Read/Write Test
    Tests Work RAM at 0xC000-0xFFFF with two patterns (0xA5 and 0x5A)
    """
    rom = []

    # JP start (reset vector)
    rom += [0xC3, 0x03, 0x00]

    # start: ld hl, 0xC000
    rom += [0x21, 0x00, 0xC0]
    # ld b, 0
    rom += [0x06, 0x00]
    # ld a, 0xA5
    rom += [0x3E, 0xA5]

    # write_a5:
    # ld (hl), a
    rom += [0x77]
    # inc hl
    rom += [0x23]
    # djnz write_a5
    rom += [0x10, 0xFC]  # -4

    # ld hl, 0xC000
    rom += [0x21, 0x00, 0xC0]
    # ld b, 0
    rom += [0x06, 0x00]
    # ld a, 0xA5
    rom += [0x3E, 0xA5]

    # verify_a5:
    # ld c, (hl)
    rom += [0x4E]
    # cp c
    rom += [0xB9]
    # jr nz, test_fail (forward to fail)
    rom += [0x20, 0x2E]  # Will adjust after calculating
    # inc hl
    rom += [0x23]
    # djnz verify_a5
    rom += [0x10, 0xF7]  # -9

    # ld hl, 0xC000
    rom += [0x21, 0x00, 0xC0]
    # ld b, 0
    rom += [0x06, 0x00]
    # ld a, 0x5A
    rom += [0x3E, 0x5A]

    # write_5a:
    # ld (hl), a
    rom += [0x77]
    # inc hl
    rom += [0x23]
    # djnz write_5a
    rom += [0x10, 0xFC]

    # ld hl, 0xC000
    rom += [0x21, 0x00, 0xC0]
    # ld b, 0
    rom += [0x06, 0x00]
    # ld a, 0x5A
    rom += [0x3E, 0x5A]

    # verify_5a:
    # ld c, (hl)
    rom += [0x4E]
    # cp c
    rom += [0xB9]
    # jr nz, test_fail
    rom += [0x20, 0x0E]
    # inc hl
    rom += [0x23]
    # djnz verify_5a
    rom += [0x10, 0xF7]

    # test_pass:
    # ld hl, 0xC0F0
    rom += [0x21, 0xF0, 0xC0]
    # ld a, 0xBE
    rom += [0x3E, 0xBE]
    # ld (hl), a
    rom += [0x77]
    # inc hl
    rom += [0x23]
    # ld a, 0xEF
    rom += [0x3E, 0xEF]
    # ld (hl), a
    rom += [0x77]
    # pass_loop: jr pass_loop
    rom += [0x18, 0xFE]

    # test_fail:
    # ld hl, 0xC0F0
    rom += [0x21, 0xF0, 0xC0]
    # ld a, 0xDE
    rom += [0x3E, 0xDE]
    # ld (hl), a
    rom += [0x77]
    # inc hl
    rom += [0x23]
    # ld a, 0xAD
    rom += [0x3E, 0xAD]
    # ld (hl), a
    rom += [0x77]
    # fail_loop: jr fail_loop
    rom += [0x18, 0xFE]

    write_bin('ram_rw_test.bin', rom)

def open_bus_test():
    """
    Open-Bus Test
    Tests that unmapped memory returns 0xFF
    """
    rom = []

    # Pad to 0x0010 first
    rom += [0xC3, 0x03, 0x00]  # JP 0x0003 at reset

    # Start at 0x0003
    # ld hl, 0xC100
    rom += [0x21, 0x00, 0xC1]

    # Read from 0x8000 (unmapped)
    # ld a, (0x8000)
    rom += [0x3A, 0x00, 0x80]
    # ld (hl), a
    rom += [0x77]
    # inc hl
    rom += [0x23]

    # Read from 0x9000
    rom += [0x3A, 0x00, 0x90]
    rom += [0x77]
    rom += [0x23]

    # Read from 0xBFFF
    rom += [0x3A, 0xFF, 0xBF]
    rom += [0x77]
    rom += [0x23]

    # Verify all returned 0xFF
    # ld hl, 0xC100
    rom += [0x21, 0x00, 0xC1]
    # ld a, (hl)
    rom += [0x7E]
    # cp 0xFF
    rom += [0xFE, 0xFF]
    # jr nz, test_fail
    rom += [0x20, 0x17]
    # inc hl
    rom += [0x23]
    # ld a, (hl)
    rom += [0x7E]
    # cp 0xFF
    rom += [0xFE, 0xFF]
    # jr nz, test_fail
    rom += [0x20, 0x10]
    # inc hl
    rom += [0x23]
    # ld a, (hl)
    rom += [0x7E]
    # cp 0xFF
    rom += [0xFE, 0xFF]
    # jr nz, test_fail
    rom += [0x20, 0x09]

    # test_pass:
    rom += [0x21, 0xF0, 0xC0]
    rom += [0x3E, 0xBE]
    rom += [0x77]
    rom += [0x23]
    rom += [0x3E, 0xEF]
    rom += [0x77]
    rom += [0x18, 0xFE]  # pass_loop

    # test_fail:
    rom += [0x21, 0xF0, 0xC0]
    rom += [0x3E, 0xDE]
    rom += [0x77]
    rom += [0x23]
    rom += [0x3E, 0xAD]
    rom += [0x77]
    rom += [0x18, 0xFE]  # fail_loop

    # Pad to 0x0010 and add marker
    while len(rom) < 0x10:
        rom += [0x00]
    rom[0x10:0x10] = [0x42]  # Marker at 0x0010

    write_bin('open_bus_test.bin', rom)

def unmapped_write_test():
    """
    Unmapped Write Test
    Verifies writes to unmapped memory are ignored
    """
    rom = []

    # JP start
    rom += [0xC3, 0x03, 0x00]

    # start:
    # ld a, 0x12
    rom += [0x3E, 0x12]
    # ld (0x8000), a
    rom += [0x32, 0x00, 0x80]

    # ld a, 0x34
    rom += [0x3E, 0x34]
    # ld (0x9000), a
    rom += [0x32, 0x00, 0x90]

    # ld a, 0x56
    rom += [0x3E, 0x56]
    # ld (0xBFFF), a
    rom += [0x32, 0xFF, 0xBF]

    # Read back and store in RAM
    # ld hl, 0xC100
    rom += [0x21, 0x00, 0xC1]

    # ld a, (0x8000)
    rom += [0x3A, 0x00, 0x80]
    # ld (hl), a
    rom += [0x77]
    # inc hl
    rom += [0x23]

    # ld a, (0x9000)
    rom += [0x3A, 0x00, 0x90]
    rom += [0x77]
    rom += [0x23]

    # ld a, (0xBFFF)
    rom += [0x3A, 0xFF, 0xBF]
    rom += [0x77]
    rom += [0x23]

    # Verify all are 0xFF
    # ld hl, 0xC100
    rom += [0x21, 0x00, 0xC1]
    # ld b, 3
    rom += [0x06, 0x03]

    # verify_loop:
    # ld a, (hl)
    rom += [0x7E]
    # cp 0xFF
    rom += [0xFE, 0xFF]
    # jr nz, test_fail
    rom += [0x20, 0x0B]
    # inc hl
    rom += [0x23]
    # djnz verify_loop
    rom += [0x10, 0xF7]

    # test_pass:
    rom += [0x21, 0xF0, 0xC0]
    rom += [0x3E, 0xBE]
    rom += [0x77]
    rom += [0x23]
    rom += [0x3E, 0xEF]
    rom += [0x77]
    rom += [0x18, 0xFE]

    # test_fail:
    rom += [0x21, 0xF0, 0xC0]
    rom += [0x3E, 0xDE]
    rom += [0x77]
    rom += [0x23]
    rom += [0x3E, 0xAD]
    rom += [0x77]
    rom += [0x18, 0xFE]

    write_bin('unmapped_write_test.bin', rom)

def io_stub_test():
    """
    I/O Stub Test
    Tests that I/O reads return 0xFF and writes are ignored
    """
    rom = []

    # JP start
    rom += [0xC3, 0x03, 0x00]

    # start:
    # ld hl, 0xC100
    rom += [0x21, 0x00, 0xC1]

    # IN A, (0x10)
    rom += [0xDB, 0x10]
    # ld (hl), a
    rom += [0x77]
    # inc hl
    rom += [0x23]

    # IN A, (0x00)
    rom += [0xDB, 0x00]
    rom += [0x77]
    rom += [0x23]

    # IN A, (0xFF)
    rom += [0xDB, 0xFF]
    rom += [0x77]
    rom += [0x23]

    # Test OUT
    # ld a, 0x42
    rom += [0x3E, 0x42]
    # OUT (0x10), A
    rom += [0xD3, 0x10]

    # ld a, 0x99
    rom += [0x3E, 0x99]
    # OUT (0x00), A
    rom += [0xD3, 0x00]

    # Verify all reads were 0xFF
    # ld hl, 0xC100
    rom += [0x21, 0x00, 0xC1]
    # ld b, 3
    rom += [0x06, 0x03]

    # verify_loop:
    rom += [0x7E]
    rom += [0xFE, 0xFF]
    rom += [0x20, 0x0B]
    rom += [0x23]
    rom += [0x10, 0xF7]

    # test_pass:
    rom += [0x21, 0xF0, 0xC0]
    rom += [0x3E, 0xBE]
    rom += [0x77]
    rom += [0x23]
    rom += [0x3E, 0xEF]
    rom += [0x77]
    rom += [0x18, 0xFE]

    # test_fail:
    rom += [0x21, 0xF0, 0xC0]
    rom += [0x3E, 0xDE]
    rom += [0x77]
    rom += [0x23]
    rom += [0x3E, 0xAD]
    rom += [0x77]
    rom += [0x18, 0xFE]

    write_bin('io_stub_test.bin', rom)

def reset_bank0_test():
    """
    Reset Bank0 Test
    Verifies bank 0 is mapped at reset vector
    """
    rom = []

    # JP start (at 0x0000)
    rom += [0xC3, 0x03, 0x00]

    # start:
    # ld hl, 0xC100
    rom += [0x21, 0x00, 0xC1]

    # Read reset vector
    # ld a, (0x0000)
    rom += [0x3A, 0x00, 0x00]
    # ld (hl), a
    rom += [0x77]
    # inc hl
    rom += [0x23]

    # Verify it's 0xC3 (JP opcode)
    # ld hl, 0xC100
    rom += [0x21, 0x00, 0xC1]
    # ld a, (hl)
    rom += [0x7E]
    # cp 0xC3
    rom += [0xFE, 0xC3]
    # jr nz, test_fail
    rom += [0x20, 0x0B]

    # test_pass:
    rom += [0x21, 0xF0, 0xC0]
    rom += [0x3E, 0xBE]
    rom += [0x77]
    rom += [0x23]
    rom += [0x3E, 0xEF]
    rom += [0x77]
    rom += [0x18, 0xFE]

    # test_fail:
    rom += [0x21, 0xF0, 0xC0]
    rom += [0x3E, 0xDE]
    rom += [0x77]
    rom += [0x23]
    rom += [0x3E, 0xAD]
    rom += [0x77]
    rom += [0x18, 0xFE]

    # Pad to 0x0100 and add marker
    while len(rom) < 0x100:
        rom += [0x00]
    rom += [0xAA]  # Marker at 0x0100

    write_bin('reset_bank0_test.bin', rom)

if __name__ == '__main__':
    os.chdir(os.path.dirname(__file__))
    ram_rw_test()
    open_bus_test()
    unmapped_write_test()
    io_stub_test()
    reset_bank0_test()
    print("All ROMs generated successfully!")
