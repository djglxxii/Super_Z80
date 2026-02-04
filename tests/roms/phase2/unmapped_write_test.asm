; Unmapped Write Test for Phase 2
; Verifies writes to unmapped memory are ignored

    .org 0x0000

start:
    ; Attempt writes to unmapped memory (0x8000-0xBFFF)
    ld a, 0x12
    ld (0x8000), a      ; Write to unmapped

    ld a, 0x34
    ld (0x9000), a      ; Write to unmapped

    ld a, 0x56
    ld (0xBFFF), a      ; Write to unmapped

    ; Read back from those addresses - should still be 0xFF (open-bus)
    ld hl, 0xC100       ; Store results in Work RAM

    ld a, (0x8000)
    ld (hl), a
    inc hl

    ld a, (0x9000)
    ld (hl), a
    inc hl

    ld a, (0xBFFF)
    ld (hl), a
    inc hl

    ; Verify all reads still return 0xFF (writes were ignored)
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
