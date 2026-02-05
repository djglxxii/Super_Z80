; Phase 5 VBlank Timing Test ROM
; Super_Z80 Emulator Bring-Up
;
; Purpose: Validate VBlank IRQ fires exactly once per frame
;
; Test behavior:
; - Configure IM 1 interrupt mode (vector to 0x0038)
; - Enable VBlank IRQ source (IRQ_ENABLE port 0x81, bit 0)
; - ISR increments counter and ACKs VBlank pending
; - Main loop busy-waits forever (no HALT)
;
; Expected outcome:
; - ISR_VBLANK_COUNT increments once per frame
; - VBlank flag (VDP_STATUS bit 0) toggles at scanlines 0 and 192
; - /INT asserts at scanline 192, drops after IRQ_ACK

; I/O Port Definitions
IRQ_STATUS  equ 0x80    ; R: IRQ pending bits
IRQ_ENABLE  equ 0x81    ; R/W: IRQ enable mask
IRQ_ACK     equ 0x82    ; W: write-1-to-clear pending bits
VDP_STATUS  equ 0x10    ; R: VDP status (bit 0 = VBlank)

; RAM Locations (Work RAM: 0xC000-0xFFFF)
ISR_VBLANK_COUNT    equ 0xC000  ; u16: incremented in ISR
FRAME_COUNT_MAIN    equ 0xC002  ; u16: incremented in main loop
LAST_VDP_STATUS     equ 0xC004  ; u8: for transition detection (optional)

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

    ld hl, FRAME_COUNT_MAIN
    ld (hl), 0x00
    inc hl
    ld (hl), 0x00           ; FRAME_COUNT_MAIN = 0

    ld a, 0x00
    ld (LAST_VDP_STATUS), a ; LAST_VDP_STATUS = 0

    ; Set interrupt mode 1 (vector to 0x0038)
    im 1

    ; Enable VBlank IRQ source
    ; IRQ_ENABLE bit 0 = 1 (VBlank), all others = 0
    ld a, 0x01
    out (IRQ_ENABLE), a

    ; Enable interrupts (EI delay honored by CPU core)
    ei

    ; Fall through to main loop
    jp main_loop

; ISR at 0x0038 (IM 1 vector)
    org 0x0038
isr_vblank:
    push af                 ; Save registers
    push hl

    ; Increment ISR_VBLANK_COUNT (16-bit)
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

    ; Acknowledge VBlank pending (IRQ_ACK port 0x82, bit 0)
    ld a, 0x01
    out (IRQ_ACK), a

    pop hl                  ; Restore registers
    pop af
    reti                    ; Return from interrupt

; Main loop (busy-wait forever)
main_loop:
    ; Option A (ISR-driven): just loop and let ISR do the work
    ; No HALT to avoid masking timing bugs

    ; Optional: Poll VDP_STATUS for VBlank transitions
    in a, (VDP_STATUS)
    ld hl, LAST_VDP_STATUS
    ld b, (hl)              ; Load previous status
    ld (hl), a              ; Store current status
    xor b                   ; Check for change
    and 0x01                ; Isolate VBlank bit
    jr z, main_loop         ; No transition, loop

    ; VBlank transition detected, increment FRAME_COUNT_MAIN (optional)
    ld hl, FRAME_COUNT_MAIN
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
; For a simple test, 256 bytes is sufficient
    org 0x00FF
    db 0x00
