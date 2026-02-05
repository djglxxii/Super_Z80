; Phase 6 DMA Test ROM
; Super_Z80 Emulator Bring-Up
;
; Purpose: Validate DMA transfer functionality during VBlank
;
; Test cases:
; 1. Positive: DMA during VBlank copies RAM pattern to VRAM
; 2. Negative: DMA outside VBlank with QUEUE=0 does nothing
; 3. Queue: DMA outside VBlank with QUEUE=1 executes at next VBlank
;
; Expected outcome:
; - VRAM contains expected patterns after successful DMA
; - DMA respects VBlank timing constraints
; - Queued DMA executes exactly at scanline 192

; I/O Port Definitions
IRQ_STATUS  equ 0x80    ; R: IRQ pending bits
IRQ_ENABLE  equ 0x81    ; R/W: IRQ enable mask
IRQ_ACK     equ 0x82    ; W: write-1-to-clear pending bits
VDP_STATUS  equ 0x10    ; R: VDP status (bit 0 = VBlank)

; DMA I/O Ports (Phase 6)
DMA_SRC_LO  equ 0x30    ; R/W: Source address low byte (CPU space)
DMA_SRC_HI  equ 0x31    ; R/W: Source address high byte
DMA_DST_LO  equ 0x32    ; R/W: Destination address low byte (VRAM)
DMA_DST_HI  equ 0x33    ; R/W: Destination address high byte
DMA_LEN_LO  equ 0x34    ; R/W: Length low byte
DMA_LEN_HI  equ 0x35    ; R/W: Length high byte
DMA_CTRL    equ 0x36    ; R/W: Control register
                        ; bit 0 = START (write-1 to trigger)
                        ; bit 1 = QUEUE_IF_NOT_VBLANK
                        ; bit 7 = BUSY (read-only, always 0)

; DMA_CTRL bits
DMA_START           equ 0x01
DMA_QUEUE           equ 0x02

; RAM Locations (Work RAM: 0xC000-0xFFFF)
ISR_VBLANK_COUNT    equ 0xC000  ; u16: incremented in ISR
TEST_PATTERN_BUF    equ 0xC100  ; 256 bytes: source buffer for DMA
TEST_STATE          equ 0xC002  ; u8: current test state (0=init, 1=test1, etc.)

; Reset vector (0x0000)
    org 0x0000
reset:
    di                      ; Disable interrupts during setup
    ld sp, 0xFFFE           ; Initialize stack pointer near top of RAM

    ; Clear counters
    ld hl, ISR_VBLANK_COUNT
    ld (hl), 0x00
    inc hl
    ld (hl), 0x00           ; ISR_VBLANK_COUNT = 0

    ; Initialize test state
    ld a, 0x00
    ld (TEST_STATE), a

    ; Fill TEST_PATTERN_BUF with incrementing pattern (0x00..0xFF)
    ld hl, TEST_PATTERN_BUF
    ld b, 0                 ; Counter: 256 iterations
fill_loop:
    ld a, b
    ld (hl), a
    inc hl
    djnz fill_loop          ; Decrement B, loop if not zero

    ; Set interrupt mode 1 (vector to 0x0038)
    im 1

    ; Enable VBlank IRQ source
    ld a, 0x01
    out (IRQ_ENABLE), a

    ; Enable interrupts
    ei

    ; Fall through to main loop
    jp main_loop

; ISR at 0x0038 (IM 1 vector)
    org 0x0038
isr_vblank:
    push af
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

    ; Acknowledge VBlank pending
    ld a, 0x01
    out (IRQ_ACK), a

    pop hl
    pop af
    reti

; Main loop: Perform DMA tests
main_loop:
    ; Load current test state
    ld a, (TEST_STATE)
    cp 0
    jr z, test0_init
    cp 1
    jr z, test1_dma_during_vblank
    cp 2
    jr z, test2_dma_mid_frame_no_queue
    cp 3
    jr z, test3_dma_mid_frame_with_queue

    ; All tests done, infinite loop
    jp main_loop

; Test 0: Wait for first VBlank (initialization)
test0_init:
    ; Wait until ISR_VBLANK_COUNT > 0
    ld hl, ISR_VBLANK_COUNT
    ld a, (hl)
    or a
    jr z, test0_init

    ; Advance to test 1
    ld a, 1
    ld (TEST_STATE), a
    jp main_loop

; Test 1: Positive DMA during VBlank
; Wait for VBlank, then program DMA to copy TEST_PATTERN_BUF to VRAM[0x2000]
test1_dma_during_vblank:
    ; Wait for VBlank flag to be set
    in a, (VDP_STATUS)
    and 0x01
    jr z, test1_dma_during_vblank

    ; We're in VBlank, program DMA
    ; SRC = TEST_PATTERN_BUF (0xC100)
    ld a, 0x00
    out (DMA_SRC_LO), a
    ld a, 0xC1
    out (DMA_SRC_HI), a

    ; DST = 0x2000 (VRAM)
    ld a, 0x00
    out (DMA_DST_LO), a
    ld a, 0x20
    out (DMA_DST_HI), a

    ; LEN = 256 (0x0100)
    ld a, 0x00
    out (DMA_LEN_LO), a
    ld a, 0x01
    out (DMA_LEN_HI), a

    ; CTRL = START (no queue, we're in VBlank)
    ld a, DMA_START
    out (DMA_CTRL), a

    ; DMA should execute immediately (instantaneous in emulation)
    ; VRAM[0x2000..0x20FF] should now contain pattern 0x00..0xFF

    ; Advance to test 2
    ld a, 2
    ld (TEST_STATE), a
    jp main_loop

; Test 2: Negative - DMA mid-frame with QUEUE=0 (should be ignored)
test2_dma_mid_frame_no_queue:
    ; Wait until we're NOT in VBlank
    in a, (VDP_STATUS)
    and 0x01
    jr nz, test2_dma_mid_frame_no_queue

    ; We're in visible scanlines, program DMA without QUEUE bit
    ; SRC = TEST_PATTERN_BUF + 1 (0xC101)
    ld a, 0x01
    out (DMA_SRC_LO), a
    ld a, 0xC1
    out (DMA_SRC_HI), a

    ; DST = 0x3000 (VRAM)
    ld a, 0x00
    out (DMA_DST_LO), a
    ld a, 0x30
    out (DMA_DST_HI), a

    ; LEN = 128 (0x0080)
    ld a, 0x80
    out (DMA_LEN_LO), a
    ld a, 0x00
    out (DMA_LEN_HI), a

    ; CTRL = START (no queue bit)
    ld a, DMA_START
    out (DMA_CTRL), a

    ; DMA should be ignored (not in VBlank, no queue)
    ; VRAM[0x3000..0x307F] should remain unchanged (zeros)

    ; Advance to test 3
    ld a, 3
    ld (TEST_STATE), a
    jp main_loop

; Test 3: Queue - DMA mid-frame with QUEUE=1 (should execute at next VBlank)
test3_dma_mid_frame_with_queue:
    ; Wait until we're NOT in VBlank
    in a, (VDP_STATUS)
    and 0x01
    jr nz, test3_dma_mid_frame_with_queue

    ; We're in visible scanlines, program DMA with QUEUE bit
    ; SRC = TEST_PATTERN_BUF (0xC100)
    ld a, 0x00
    out (DMA_SRC_LO), a
    ld a, 0xC1
    out (DMA_SRC_HI), a

    ; DST = 0x4000 (VRAM)
    ld a, 0x00
    out (DMA_DST_LO), a
    ld a, 0x40
    out (DMA_DST_HI), a

    ; LEN = 256 (0x0100)
    ld a, 0x00
    out (DMA_LEN_LO), a
    ld a, 0x01
    out (DMA_LEN_HI), a

    ; CTRL = START | QUEUE_IF_NOT_VBLANK
    ld a, DMA_START | DMA_QUEUE
    out (DMA_CTRL), a

    ; DMA should be queued and execute at next VBlank (scanline 192)
    ; VRAM[0x4000..0x40FF] should contain pattern 0x00..0xFF after next VBlank

    ; Wait for next VBlank to ensure queued DMA completes
    ld hl, ISR_VBLANK_COUNT
    ld b, (hl)              ; Save current VBlank count
test3_wait_next_vblank:
    ld hl, ISR_VBLANK_COUNT
    ld a, (hl)
    cp b
    jr z, test3_wait_next_vblank

    ; Queued DMA should have executed
    ; All tests complete, advance to state 4 (done)
    ld a, 4
    ld (TEST_STATE), a
    jp main_loop

; Padding to fill ROM
    org 0x03FF
    db 0x00
