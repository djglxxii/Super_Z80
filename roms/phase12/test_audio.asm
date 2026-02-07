; Phase 12 Audio Test ROM
; Super_Z80 Emulator Bring-Up
;
; Purpose: Validate PSG and YM2151 audio subsystems
;
; Test sequence:
; 1. Initialize interrupts
; 2. Play a PSG tone (440Hz A4), then change pitch after delay
; 3. Play a YM2151 note (simple sine, algorithm 7)
; 4. Loop forever (PSG + YM2151 continue playing)
;
; Expected audible result:
; - PSG tone stable, then pitch change
; - YM2151 produces a stable note

; =====================================================
; I/O Port Definitions
; =====================================================
IRQ_STATUS  equ 0x80
IRQ_ENABLE  equ 0x81
IRQ_ACK     equ 0x82

; PSG (SN76489)
PSG_DATA    equ 0x60

; YM2151 (OPM)
OPM_ADDR    equ 0x70
OPM_DATA    equ 0x71

; Master audio
AUDIO_MASTER_VOL equ 0x7C

; RAM
FRAME_COUNT equ 0xC000
TEST_STATE  equ 0xC002

; =====================================================
; ROM Entry Point (0x0000)
; =====================================================
    org 0x0000

    di                      ; Disable interrupts during setup
    ld sp, 0xFFFF           ; Set stack pointer to top of RAM
    jp init

; =====================================================
; Interrupt Vector (0x0038 - IM 1 handler)
; =====================================================
    org 0x0038
isr:
    push af
    push hl

    ; Acknowledge VBlank IRQ
    in a, (IRQ_STATUS)
    out (IRQ_ACK), a

    ; Increment frame counter
    ld hl, (FRAME_COUNT)
    inc hl
    ld (FRAME_COUNT), hl

    pop hl
    pop af
    ei
    reti

; =====================================================
; Initialization
; =====================================================
    org 0x0080
init:
    ; Clear RAM variables
    xor a
    ld (FRAME_COUNT), a
    ld (FRAME_COUNT+1), a
    ld (TEST_STATE), a

    ; Set master volume to max
    ld a, 0xFF
    out (AUDIO_MASTER_VOL), a

    ; Enable VBlank IRQ
    ld a, 0x01
    out (IRQ_ENABLE), a
    im 1
    ei

    ; =====================================================
    ; Step 1: PSG Tone - Play 440Hz (A4)
    ; SN76489 frequency formula: f = clock / (2 * period * 16)
    ; For PSG_HZ=3579545, period for 440Hz:
    ;   period = 3579545 / (2 * 440 * 16) = 254 = 0x0FE
    ;
    ; Latch/data format: 1 cc t dddd
    ;   Channel 0 tone: cc=00, t=0 -> 0x80 | (low 4 bits)
    ;   Data byte:       0 x dddddd -> high 6 bits
    ; =====================================================

    ; Set Channel 0 volume to max (0 = loudest)
    ld a, 0x90              ; Latch ch0 volume, data=0 (max vol)
    out (PSG_DATA), a

    ; Set Channel 0 tone period = 0x0FE (254)
    ; Low 4 bits: 0x0E
    ld a, 0x80 | 0x0E       ; Latch ch0 tone, low nibble = 0xE
    out (PSG_DATA), a
    ; High 6 bits: 0x0F (0xFE >> 4 = 0x0F)
    ld a, 0x0F               ; Data byte, high 6 bits
    out (PSG_DATA), a

    ; Silence other PSG channels
    ld a, 0xBF              ; Ch1 vol = 0xF (off)
    out (PSG_DATA), a
    ld a, 0xDF              ; Ch2 vol = 0xF (off)
    out (PSG_DATA), a
    ld a, 0xFF              ; Noise vol = 0xF (off)
    out (PSG_DATA), a

    ; =====================================================
    ; Step 2: Wait ~2 seconds (120 frames at 60Hz), then change PSG pitch
    ; =====================================================
    call wait_120_frames

    ; Change to ~880Hz (A5): period = 127 = 0x07F
    ld a, 0x80 | 0x0F       ; Latch ch0 tone, low nibble = 0xF
    out (PSG_DATA), a
    ld a, 0x07               ; Data byte, high 6 bits
    out (PSG_DATA), a

    ; =====================================================
    ; Step 3: YM2151 - Play a simple tone on channel 0
    ;
    ; Use algorithm 7 (all operators output directly)
    ; Only enable Operator 1 (M1) with a simple setup
    ; =====================================================

    ; Channel 0 control: RL=3 (both), FB=0, CON=7 (algorithm 7)
    ld a, 0x20              ; Register: Channel 0 RL/FB/CON
    out (OPM_ADDR), a
    ld a, 0xC7              ; RL=11, FB=000, CON=111
    out (OPM_DATA), a

    ; DT1/MUL for Operator M1 (slot 0, ch 0) = reg 0x40
    ld a, 0x40
    out (OPM_ADDR), a
    ld a, 0x01              ; DT1=0, MUL=1
    out (OPM_DATA), a

    ; TL (Total Level) for M1 = reg 0x60
    ; Lower = louder (0 = max volume)
    ld a, 0x60
    out (OPM_ADDR), a
    ld a, 0x10              ; TL = 0x10 (moderate volume)
    out (OPM_DATA), a

    ; KS/AR for M1 = reg 0x80
    ld a, 0x80
    out (OPM_ADDR), a
    ld a, 0x1F              ; KS=0, AR=31 (fastest attack)
    out (OPM_DATA), a

    ; D1R for M1 = reg 0xA0
    ld a, 0xA0
    out (OPM_ADDR), a
    ld a, 0x00              ; D1R=0 (no decay)
    out (OPM_DATA), a

    ; D2R for M1 = reg 0xC0
    ld a, 0xC0
    out (OPM_ADDR), a
    ld a, 0x00              ; D2R=0 (sustain forever)
    out (OPM_DATA), a

    ; D1L/RR for M1 = reg 0xE0
    ld a, 0xE0
    out (OPM_ADDR), a
    ld a, 0x0F              ; D1L=0, RR=15 (fast release)
    out (OPM_DATA), a

    ; Silence other operators by setting TL to max (0x7F)
    ; M2 (slot 1) TL = reg 0x68
    ld a, 0x68
    out (OPM_ADDR), a
    ld a, 0x7F
    out (OPM_DATA), a
    ; C1 (slot 2) TL = reg 0x70
    ld a, 0x70
    out (OPM_ADDR), a
    ld a, 0x7F
    out (OPM_DATA), a
    ; C2 (slot 3) TL = reg 0x78
    ld a, 0x78
    out (OPM_ADDR), a
    ld a, 0x7F
    out (OPM_DATA), a

    ; Set KC (Key Code) for channel 0 = reg 0x28
    ; Octave 4, note A (KC = 0x4A)
    ld a, 0x28
    out (OPM_ADDR), a
    ld a, 0x4A              ; Octave 4, note 10 (A)
    out (OPM_DATA), a

    ; Key On: channel 0, operator M1 (bit 3)
    ld a, 0x08              ; Key On/Off register
    out (OPM_ADDR), a
    ld a, 0x78              ; SN=0111 (M1+C1+C2 on), CH=0 -- simplify: all ops on
    out (OPM_DATA), a

    ; =====================================================
    ; Main loop: wait forever (PSG + YM2151 continue playing)
    ; =====================================================
main_loop:
    call wait_120_frames
    jr main_loop

; =====================================================
; Subroutine: Wait approximately 120 frames (~2 seconds at 60Hz)
; Uses the frame counter incremented by VBlank ISR
; =====================================================
wait_120_frames:
    ld hl, (FRAME_COUNT)
    ld de, 120
    add hl, de
    ; HL = target frame count
.wait_loop:
    halt                    ; Wait for next VBlank
    push hl
    ld de, (FRAME_COUNT)
    ; Compare DE >= HL
    or a                    ; Clear carry
    sbc hl, de
    pop hl
    jr nc, .wait_loop       ; If HL > FRAME_COUNT, keep waiting
    ret
