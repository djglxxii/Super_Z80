; I/O Stub Test for Phase 2
; Tests that I/O reads return 0xFF and writes are ignored

    .org 0x0000

start:
    ; Test I/O reads from various ports
    ld hl, 0xC100       ; Store results in Work RAM

    ; IN from port 0x10
    in a, (0x10)
    ld (hl), a
    inc hl

    ; IN from port 0x00
    in a, (0x00)
    ld (hl), a
    inc hl

    ; IN from port 0xFF
    in a, (0xFF)
    ld (hl), a
    inc hl

    ; Test I/O writes (no observable effect, but should not crash)
    ld a, 0x42
    out (0x10), a

    ld a, 0x99
    out (0x00), a

    ; Verify all I/O reads returned 0xFF
    ld hl, 0xC100
    ld b, 3             ; 3 values to check

verify_loop:
    ld a, (hl)
    cp 0xFF
    jr nz, test_fail
    inc hl
    djnz verify_loop

test_pass:
    ; Write PASS signature to 0xC0F0
    ld hl, 0xC0F0
    ld a, 0xBE
    ld (hl), a
    inc hl
    ld a, 0xEF
    ld (hl), a

pass_loop:
    jr pass_loop

test_fail:
    ; Write FAIL signature to 0xC0F0
    ld hl, 0xC0F0
    ld a, 0xDE
    ld (hl), a
    inc hl
    ld a, 0xAD
    ld (hl), a

fail_loop:
    jr fail_loop
