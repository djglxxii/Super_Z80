; RAM Read/Write Test for Phase 2
; Tests Work RAM at 0xC000-0xFFFF with two patterns

    .org 0x0000

start:
    ; Test pattern 0xA5
    ld hl, 0xC000       ; Start of Work RAM
    ld b, 0             ; Counter (256 bytes)
    ld a, 0xA5          ; Pattern A5h

write_a5:
    ld (hl), a          ; Write pattern
    inc hl
    djnz write_a5       ; Decrement B and loop

    ; Verify pattern 0xA5
    ld hl, 0xC000
    ld b, 0
    ld a, 0xA5

verify_a5:
    ld c, (hl)          ; Read back
    cp c                ; Compare with expected
    jr nz, test_fail    ; Mismatch -> fail
    inc hl
    djnz verify_a5

    ; Test pattern 0x5A
    ld hl, 0xC000
    ld b, 0
    ld a, 0x5A

write_5a:
    ld (hl), a
    inc hl
    djnz write_5a

    ; Verify pattern 0x5A
    ld hl, 0xC000
    ld b, 0
    ld a, 0x5A

verify_5a:
    ld c, (hl)
    cp c
    jr nz, test_fail
    inc hl
    djnz verify_5a

test_pass:
    ; Write PASS signature (0xBEEF) to 0xC0F0
    ld hl, 0xC0F0
    ld a, 0xBE
    ld (hl), a
    inc hl
    ld a, 0xEF
    ld (hl), a

    ; Infinite loop at known address
pass_loop:
    jr pass_loop

test_fail:
    ; Write FAIL signature (0xDEAD) to 0xC0F0
    ld hl, 0xC0F0
    ld a, 0xDE
    ld (hl), a
    inc hl
    ld a, 0xAD
    ld (hl), a

fail_loop:
    jr fail_loop
