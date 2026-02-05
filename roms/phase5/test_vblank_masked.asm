; Phase 5 VBlank Timing Test ROM (Negative Test - Masked)
; Super_Z80 Emulator Bring-Up
;
; Purpose: Validate VBlank flag toggles even when IRQ is masked
;
; Test behavior:
; - Configure IM 1 interrupt mode (vector to 0x0038)
; - Leave VBlank IRQ DISABLED (IRQ_ENABLE bit 0 = 0)
; - Poll VDP_STATUS to confirm VBlank flag toggles
; - Verify ISR never runs (ISR_VBLANK_COUNT stays 0)
;
; Expected outcome:
; - VBlank flag (VDP_STATUS bit 0) toggles at scanlines 0 and 192
; - ISR_VBLANK_COUNT remains 0 (ISR never executed)
; - /INT never asserts (VBlank is masked)

; I/O Port Definitions
IRQ_STATUS  equ 0x80    ; R: IRQ pending bits
IRQ_ENABLE  equ 0x81    ; R/W: IRQ enable mask
IRQ_ACK     equ 0x82    ; W: write-1-to-clear pending bits
VDP_STATUS  equ 0x10    ; R: VDP status (bit 0 = VBlank)

; RAM Locations (Work RAM: 0xC000-0xFFFF)
ISR_VBLANK_COUNT    equ 0xC000  ; u16: should stay 0
VBLANK_TOGGLE_COUNT equ 0xC002  ; u16: incremented on VBlank transitions
LAST_VDP_STATUS     equ 0xC004  ; u8: for transition detection

; Reset vector (0x0000)
    org 0x0000
reset:
    di                      ; Disable interrupts during setup
    ld sp, 0xFFFE           ; Initialize stack pointer near top of RAM

    ; Clear counters in RAM
    ld hl, ISR_VBLANK_COUNT
    ld (hl), 0x00
    inc hl
    ld (hl), 0x00           ; ISR_VBLANK_COUNT = 0

    ld hl, VBLANK_TOGGLE_COUNT
    ld (hl), 0x00
    inc hl
    ld (hl), 0x00           ; VBLANK_TOGGLE_COUNT = 0

    ld a, 0x00
    ld (LAST_VDP_STATUS), a ; LAST_VDP_STATUS = 0

    ; Set interrupt mode 1 (vector to 0x0038)
    im 1

    ; LEAVE VBlank IRQ DISABLED (IRQ_ENABLE = 0x00)
    ld a, 0x00
    out (IRQ_ENABLE), a

    ; Enable interrupts (even though VBlank is masked, for completeness)
    ei

    ; Fall through to main loop
    jp main_loop

; ISR at 0x0038 (IM 1 vector)
; This should NEVER be called in this test
    org 0x0038
isr_vblank:
    push af                 ; Save registers
    push hl

    ; If we get here, something is wrong! Increment ISR_VBLANK_COUNT anyway
    ld hl, ISR_VBLANK_COUNT
    ld a, (hl)
    inc a
    ld (hl), a
    jr nz, isr_no_carry
    inc hl
    ld a, (hl)
    inc a
    ld (hl), a
isr_no_carry:

    ; Acknowledge VBlank pending (shouldn't be needed, but be safe)
    ld a, 0x01
    out (IRQ_ACK), a

    pop hl                  ; Restore registers
    pop af
    reti                    ; Return from interrupt

; Main loop (poll VDP_STATUS for VBlank transitions)
main_loop:
    ; Poll VDP_STATUS
    in a, (VDP_STATUS)
    ld hl, LAST_VDP_STATUS
    ld b, (hl)              ; Load previous status
    ld (hl), a              ; Store current status
    xor b                   ; Check for change
    and 0x01                ; Isolate VBlank bit
    jr z, main_loop         ; No transition, loop

    ; VBlank transition detected (0->1 or 1->0)
    ; Increment VBLANK_TOGGLE_COUNT
    ld hl, VBLANK_TOGGLE_COUNT
    ld a, (hl)
    inc a
    ld (hl), a
    jr nz, main_loop_no_carry
    inc hl
    ld a, (hl)
    inc a
    ld (hl), a
main_loop_no_carry:

    jp main_loop            ; Loop forever

; Padding to fill ROM (optional, depending on ROM size requirements)
    org 0x00FF
    db 0x00
