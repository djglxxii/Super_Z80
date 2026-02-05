; Phase 8 Palette Test ROM
; Super_Z80 Emulator Bring-Up
;
; Purpose: Validate Palette RAM, Palette I/O, Palette DMA, and timing
;
; Test cases:
; 1. Initialize palette via DMA during VBlank
; 2. Display tiles using palette colors 1-4
; 3. Toggle palette entry 1 (red/blue) in VBlank ISR
;
; Expected outcome:
; - Screen displays colored tiles using palette
; - Color pulses once per frame (no tearing)
; - Palette changes only visible at next scanline boundary

; I/O Port Definitions
IRQ_STATUS  equ 0x80    ; R: IRQ pending bits
IRQ_ENABLE  equ 0x81    ; R/W: IRQ enable mask
IRQ_ACK     equ 0x82    ; W: write-1-to-clear pending bits
VDP_STATUS  equ 0x10    ; R: VDP status (bit 0 = VBlank)

; PPU Registers
VDP_CTRL        equ 0x11    ; R/W: Display control (bit 0 = display enable)
SCROLL_X        equ 0x12    ; R/W: Plane A scroll X (pixels)
SCROLL_Y        equ 0x13    ; R/W: Plane A scroll Y (pixels)
PLANE_A_BASE    equ 0x16    ; R/W: Tilemap base (page index, *1024)
PATTERN_BASE    equ 0x18    ; R/W: Pattern base (page index, *1024)

; Palette I/O Ports (Phase 8)
PAL_ADDR        equ 0x1E    ; R/W: Palette byte address (0-255)
PAL_DATA        equ 0x1F    ; R/W: Palette data byte (auto-increment)

; DMA I/O Ports
DMA_SRC_LO  equ 0x30    ; R/W: Source address low byte (CPU space)
DMA_SRC_HI  equ 0x31    ; R/W: Source address high byte
DMA_DST_LO  equ 0x32    ; R/W: Destination address low byte
DMA_DST_HI  equ 0x33    ; R/W: Destination address high byte
DMA_LEN_LO  equ 0x34    ; R/W: Length low byte
DMA_LEN_HI  equ 0x35    ; R/W: Length high byte
DMA_CTRL    equ 0x36    ; R/W: Control register

; DMA_CTRL bits
DMA_START       equ 0x01    ; bit 0: Start trigger
DMA_QUEUE       equ 0x02    ; bit 1: Queue if not VBlank
DMA_DST_PALETTE equ 0x08    ; bit 3: Destination is Palette RAM (Phase 8)

; VRAM Layout
VRAM_PATTERN_BASE   equ 0x0000  ; Tile patterns at VRAM 0x0000
VRAM_TILEMAP_BASE   equ 0x1000  ; Tilemap at VRAM 0x1000 (page 4)
PATTERN_PAGE        equ 0       ; PATTERN_BASE register value
TILEMAP_PAGE        equ 4       ; PLANE_A_BASE register value

; RAM Locations (Work RAM: 0xC000-0xFFFF)
ISR_VBLANK_COUNT    equ 0xC000  ; u16: incremented in ISR
PALETTE_TOGGLE      equ 0xC002  ; u8: 0=red, 1=blue for entry 1
TEST_STATE          equ 0xC003  ; u8: current test state
TILE_DATA_BUF       equ 0xC100  ; 128 bytes: 4 tiles * 32 bytes each
TILEMAP_DATA_BUF    equ 0xC180  ; 1536 bytes: 32x24 * 2 bytes each

; Palette Data Buffer (Phase 8): 32 bytes for 16 palette entries
; Each entry is 2 bytes (little-endian, 9-bit RGB)
; Bit layout: bits 0-2=R, 3-5=G, 6-8=B
PALETTE_DATA_BUF    equ 0xC780  ; 32 bytes: 16 entries * 2 bytes

; Helper macros for 9-bit RGB packed color
; Pack RGB (each 0-7) into 16-bit: R | (G << 3) | (B << 6)
; Example: R=7, G=0, B=0 -> 0x0007 (pure red)
;          R=0, G=0, B=7 -> 0x01C0 (pure blue)

; Reset vector (0x0000)
    org 0x0000
reset:
    di                      ; Disable interrupts during setup
    ld sp, 0xFFFE           ; Initialize stack pointer

    ; Clear counters
    ld hl, ISR_VBLANK_COUNT
    ld (hl), 0x00
    inc hl
    ld (hl), 0x00

    ; Initialize state
    xor a
    ld (TEST_STATE), a
    ld (PALETTE_TOGGLE), a

    ; Generate tile patterns in RAM
    call generate_tiles

    ; Generate tilemap in RAM
    call generate_tilemap

    ; Generate palette data in RAM
    call generate_palette

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

    ; Phase 8: Toggle palette entry 1 between red and blue
    ld a, (PALETTE_TOGGLE)
    xor 1                   ; Toggle 0<->1
    ld (PALETTE_TOGGLE), a

    ; Set PAL_ADDR to entry 1 (byte address = 1*2 = 2)
    ld a, 2
    out (PAL_ADDR), a

    ; Write palette entry 1 based on toggle
    ld a, (PALETTE_TOGGLE)
    or a
    jr nz, write_blue

write_red:
    ; Red: R=7, G=0, B=0 -> packed = 0x0007
    ld a, 0x07              ; Low byte
    out (PAL_DATA), a
    xor a                   ; High byte = 0
    out (PAL_DATA), a
    jr isr_done_palette

write_blue:
    ; Blue: R=0, G=0, B=7 -> packed = 0x01C0
    ld a, 0xC0              ; Low byte (B bits 0-1 in position 6-7)
    out (PAL_DATA), a
    ld a, 0x01              ; High byte (B bit 2 in position 0)
    out (PAL_DATA), a

isr_done_palette:
    ; Acknowledge VBlank pending
    ld a, 0x01
    out (IRQ_ACK), a

    pop hl
    pop af
    reti

; Main loop
    org 0x0080
main_loop:
    ld a, (TEST_STATE)
    cp 0
    jr z, state0_wait_vblank
    cp 1
    jr z, state1_dma_tiles
    cp 2
    jr z, state2_dma_tilemap
    cp 3
    jr z, state3_dma_palette
    cp 4
    jr z, state4_setup_ppu
    cp 5
    jr z, state5_running

    jp main_loop

; State 0: Wait for first VBlank
state0_wait_vblank:
    ld hl, ISR_VBLANK_COUNT
    ld a, (hl)
    or a
    jr z, state0_wait_vblank

    ld a, 1
    ld (TEST_STATE), a
    jp main_loop

; State 1: DMA tile patterns to VRAM
state1_dma_tiles:
    in a, (VDP_STATUS)
    and 0x01
    jr z, state1_dma_tiles

    ; DMA: 128 bytes from TILE_DATA_BUF to VRAM 0x0000
    ld a, 0x00
    out (DMA_SRC_LO), a
    ld a, 0xC1
    out (DMA_SRC_HI), a

    ld a, 0x00
    out (DMA_DST_LO), a
    ld a, 0x00
    out (DMA_DST_HI), a

    ld a, 0x80
    out (DMA_LEN_LO), a
    ld a, 0x00
    out (DMA_LEN_HI), a

    ; CTRL = START (no palette flag = VRAM destination)
    ld a, DMA_START
    out (DMA_CTRL), a

    ld a, 2
    ld (TEST_STATE), a
    jp main_loop

; State 2: DMA tilemap to VRAM
state2_dma_tilemap:
    in a, (VDP_STATUS)
    and 0x01
    jr z, state2_dma_tilemap

    ; DMA: 1536 bytes from TILEMAP_DATA_BUF to VRAM 0x1000
    ld a, 0x80
    out (DMA_SRC_LO), a
    ld a, 0xC1
    out (DMA_SRC_HI), a

    ld a, 0x00
    out (DMA_DST_LO), a
    ld a, 0x10
    out (DMA_DST_HI), a

    ld a, 0x00
    out (DMA_LEN_LO), a
    ld a, 0x06
    out (DMA_LEN_HI), a

    ld a, DMA_START
    out (DMA_CTRL), a

    ld a, 3
    ld (TEST_STATE), a
    jp main_loop

; State 3: DMA palette to Palette RAM (Phase 8)
state3_dma_palette:
    in a, (VDP_STATUS)
    and 0x01
    jr z, state3_dma_palette

    ; DMA: 32 bytes from PALETTE_DATA_BUF to Palette RAM (byte addr 0)
    ld a, 0x80
    out (DMA_SRC_LO), a
    ld a, 0xC7
    out (DMA_SRC_HI), a

    ; DST = palette byte address 0
    ld a, 0x00
    out (DMA_DST_LO), a
    ld a, 0x00
    out (DMA_DST_HI), a

    ; LEN = 32 bytes (16 palette entries)
    ld a, 0x20
    out (DMA_LEN_LO), a
    ld a, 0x00
    out (DMA_LEN_HI), a

    ; CTRL = START | DST_IS_PALETTE (0x09)
    ld a, DMA_START | DMA_DST_PALETTE
    out (DMA_CTRL), a

    ld a, 4
    ld (TEST_STATE), a
    jp main_loop

; State 4: Setup PPU registers and enable display
state4_setup_ppu:
    ld a, PATTERN_PAGE
    out (PATTERN_BASE), a

    ld a, TILEMAP_PAGE
    out (PLANE_A_BASE), a

    xor a
    out (SCROLL_X), a
    out (SCROLL_Y), a

    ld a, 0x01
    out (VDP_CTRL), a

    ld a, 5
    ld (TEST_STATE), a
    jp main_loop

; State 5: Running - just loop (palette toggle happens in ISR)
state5_running:
    halt                    ; Wait for next interrupt
    jp state5_running

; Generate 4 test tiles in TILE_DATA_BUF
; Tile 0: Solid palette index 1 (will pulse red/blue)
; Tile 1: Solid palette index 2 (green)
; Tile 2: Solid palette index 3 (blue)
; Tile 3: Solid palette index 4 (white)
generate_tiles:
    ld hl, TILE_DATA_BUF

    ; Tile 0: Solid index 1 (0x11 = both pixels = 1)
    ld b, 32
    ld a, 0x11
tile0_loop:
    ld (hl), a
    inc hl
    djnz tile0_loop

    ; Tile 1: Solid index 2 (0x22)
    ld b, 32
    ld a, 0x22
tile1_loop:
    ld (hl), a
    inc hl
    djnz tile1_loop

    ; Tile 2: Solid index 3 (0x33)
    ld b, 32
    ld a, 0x33
tile2_loop:
    ld (hl), a
    inc hl
    djnz tile2_loop

    ; Tile 3: Solid index 4 (0x44)
    ld b, 32
    ld a, 0x44
tile3_loop:
    ld (hl), a
    inc hl
    djnz tile3_loop

    ret

; Generate 32x24 tilemap
generate_tilemap:
    ld hl, TILEMAP_DATA_BUF
    ld de, 768              ; 32x24 = 768 entries
    ld c, 0

tilemap_loop:
    ld a, c
    ld (hl), a
    inc hl
    xor a
    ld (hl), a
    inc hl

    ld a, c
    inc a
    and 0x03
    ld c, a

    dec de
    ld a, d
    or e
    jr nz, tilemap_loop

    ret

; Generate 16-entry palette in PALETTE_DATA_BUF
; Each entry is 2 bytes (little-endian, 9-bit RGB)
; Bit layout: bits 0-2=R, 3-5=G, 6-8=B
generate_palette:
    ld hl, PALETTE_DATA_BUF

    ; Entry 0: Black (R=0, G=0, B=0) -> 0x0000
    ld (hl), 0x00
    inc hl
    ld (hl), 0x00
    inc hl

    ; Entry 1: Red (R=7, G=0, B=0) -> 0x0007
    ld (hl), 0x07
    inc hl
    ld (hl), 0x00
    inc hl

    ; Entry 2: Green (R=0, G=7, B=0) -> 0x0038
    ld (hl), 0x38
    inc hl
    ld (hl), 0x00
    inc hl

    ; Entry 3: Blue (R=0, G=0, B=7) -> 0x01C0
    ld (hl), 0xC0
    inc hl
    ld (hl), 0x01
    inc hl

    ; Entry 4: White (R=7, G=7, B=7) -> 0x01FF
    ld (hl), 0xFF
    inc hl
    ld (hl), 0x01
    inc hl

    ; Entries 5-15: Black (fill remaining)
    ld b, 22                ; 11 entries * 2 bytes
    xor a
fill_remaining:
    ld (hl), a
    inc hl
    djnz fill_remaining

    ret

; Padding to fill ROM
    org 0x03FF
    db 0x00
