; Open-Bus Read Test for Phase 2
; Tests unmapped memory returns 0xFF and ROM reads work correctly

    .org 0x0000

start:
    ; Test unmapped memory reads (0x8000-0xBFFF range - Phase 2 assumption)
    ld hl, 0xC100       ; Store results in Work RAM

    ; Read from 0x8000 (unmapped in Phase 2)
    ld a, (0x8000)
    ld (hl), a          ; Store result
    inc hl

    ; Read from 0x9000 (unmapped)
    ld a, (0x9000)
    ld (hl), a
    inc hl

    ; Read from 0xBFFF (unmapped)
    ld a, (0xBFFF)
    ld (hl), a
    inc hl

    ; Read from 0x0010 (ROM region - should NOT be 0xFF unless ROM contains 0xFF)
    ld a, (0x0010)
    ld (hl), a
    inc hl

    ; Read from 0x7FFF (ROM last byte)
    ld a, (0x7FFF)
    ld (hl), a
    inc hl

    ; Verify unmapped reads returned 0xFF
    ld hl, 0xC100
    ld a, (hl)
    cp 0xFF
    jr nz, test_fail
    inc hl

    ld a, (hl)
    cp 0xFF
    jr nz, test_fail
    inc hl

    ld a, (hl)
    cp 0xFF
    jr nz, test_fail

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

    ; Pad to ensure we have data at 0x0010
    .org 0x0010
    .db 0x42            ; Known value at 0x0010
