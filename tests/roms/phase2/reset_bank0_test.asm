; Reset Bank 0 Test for Phase 2
; Verifies bank 0 is mapped at reset vector (0x0000)

    .org 0x0000

start:
    ; Read distinguishable data from ROM start
    ; This ROM should have known values at specific addresses
    ld hl, 0xC100       ; Store results in Work RAM

    ; Read from reset vector area
    ld a, (0x0000)      ; Should be 0xC3 (JP instruction)
    ld (hl), a
    inc hl

    ld a, (0x0001)      ; Low byte of jump address
    ld (hl), a
    inc hl

    ld a, (0x0002)      ; High byte of jump address
    ld (hl), a
    inc hl

    ; Read marker at 0x0100
    ld a, (0x0100)
    ld (hl), a
    inc hl

    ; Verify reset vector contains JP start (0xC3 0x06 0x00)
    ld hl, 0xC100
    ld a, (hl)
    cp 0xC3             ; JP opcode
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

    ; Place distinguishable marker at 0x0100
    .org 0x0100
    .db 0xAA            ; Marker byte
