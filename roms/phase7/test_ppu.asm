; Phase 7 PPU Test ROM
; Super_Z80 Emulator Bring-Up
;
; Purpose: Validate PPU Plane A rendering
;
; Test cases:
; 1. Load 4 test tiles (solid, checkerboard, vertical stripes, horizontal stripes)
; 2. Create 32x24 tilemap with repeating [0,1,2,3] pattern
; 3. DMA tiles and tilemap to VRAM during VBlank
; 4. Enable display
; 5. Animate scroll X to demonstrate smooth scrolling
;
; Expected outcome:
; - 4 distinct tile patterns visible in a repeating grid
; - Screen scrolls smoothly horizontally
; - No tearing or mid-frame artifacts

; I/O Port Definitions
IRQ_STATUS  equ 0x80    ; R: IRQ pending bits
IRQ_ENABLE  equ 0x81    ; R/W: IRQ enable mask
IRQ_ACK     equ 0x82    ; W: write-1-to-clear pending bits
VDP_STATUS  equ 0x10    ; R: VDP status (bit 0 = VBlank)

; PPU Registers (Phase 7)
VDP_CTRL        equ 0x11    ; R/W: Display control (bit 0 = display enable)
SCROLL_X        equ 0x12    ; R/W: Plane A scroll X (pixels)
SCROLL_Y        equ 0x13    ; R/W: Plane A scroll Y (pixels)
PLANE_A_BASE    equ 0x16    ; R/W: Tilemap base (page index, *1024)
PATTERN_BASE    equ 0x18    ; R/W: Pattern base (page index, *1024)

; DMA I/O Ports (Phase 6)
DMA_SRC_LO  equ 0x30    ; R/W: Source address low byte (CPU space)
DMA_SRC_HI  equ 0x31    ; R/W: Source address high byte
DMA_DST_LO  equ 0x32    ; R/W: Destination address low byte (VRAM)
DMA_DST_HI  equ 0x33    ; R/W: Destination address high byte
DMA_LEN_LO  equ 0x34    ; R/W: Length low byte
DMA_LEN_HI  equ 0x35    ; R/W: Length high byte
DMA_CTRL    equ 0x36    ; R/W: Control register

; DMA_CTRL bits
DMA_START   equ 0x01
DMA_QUEUE   equ 0x02

; VRAM Layout (Phase 7):
;   Pattern base = page 0 (0x0000)  - 4 tiles = 128 bytes
;   Tilemap base = page 4 (0x1000)  - 32x24 entries = 1536 bytes

VRAM_PATTERN_BASE   equ 0x0000  ; Tile patterns at VRAM 0x0000
VRAM_TILEMAP_BASE   equ 0x1000  ; Tilemap at VRAM 0x1000 (page 4)
PATTERN_PAGE        equ 0       ; PATTERN_BASE register value
TILEMAP_PAGE        equ 4       ; PLANE_A_BASE register value

; RAM Locations (Work RAM: 0xC000-0xFFFF)
ISR_VBLANK_COUNT    equ 0xC000  ; u16: incremented in ISR
CURRENT_SCROLL_X    equ 0xC002  ; u8: current scroll X value
TEST_STATE          equ 0xC003  ; u8: current test state
TILE_DATA_BUF       equ 0xC100  ; 128 bytes: 4 tiles * 32 bytes each
TILEMAP_DATA_BUF    equ 0xC180  ; 1536 bytes: 32x24 * 2 bytes each (ends at 0xC780)

; Tile definitions (packed 4bpp, 32 bytes per tile)
; Each row is 4 bytes (8 pixels)
; High nibble = left pixel, low nibble = right pixel

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

    ; Initialize state
    xor a
    ld (TEST_STATE), a
    ld (CURRENT_SCROLL_X), a

    ; Generate tile patterns in RAM
    call generate_tiles

    ; Generate tilemap in RAM
    call generate_tilemap

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

; Main loop
    org 0x0080
main_loop:
    ; Load current test state
    ld a, (TEST_STATE)
    cp 0
    jr z, state0_wait_vblank
    cp 1
    jr z, state1_dma_tiles
    cp 2
    jr z, state2_dma_tilemap
    cp 3
    jr z, state3_setup_ppu
    cp 4
    jr z, state4_scroll_loop

    ; Invalid state, stay in loop
    jp main_loop

; State 0: Wait for first VBlank
state0_wait_vblank:
    ld hl, ISR_VBLANK_COUNT
    ld a, (hl)
    or a
    jr z, state0_wait_vblank

    ; Advance to state 1
    ld a, 1
    ld (TEST_STATE), a
    jp main_loop

; State 1: DMA tile patterns to VRAM
state1_dma_tiles:
    ; Wait for VBlank
    in a, (VDP_STATUS)
    and 0x01
    jr z, state1_dma_tiles

    ; DMA tile patterns: 128 bytes from TILE_DATA_BUF to VRAM 0x0000
    ; SRC = TILE_DATA_BUF (0xC100)
    ld a, 0x00
    out (DMA_SRC_LO), a
    ld a, 0xC1
    out (DMA_SRC_HI), a

    ; DST = 0x0000 (VRAM pattern base)
    ld a, 0x00
    out (DMA_DST_LO), a
    ld a, 0x00
    out (DMA_DST_HI), a

    ; LEN = 128 (0x0080) - 4 tiles
    ld a, 0x80
    out (DMA_LEN_LO), a
    ld a, 0x00
    out (DMA_LEN_HI), a

    ; CTRL = START
    ld a, DMA_START
    out (DMA_CTRL), a

    ; Advance to state 2
    ld a, 2
    ld (TEST_STATE), a
    jp main_loop

; State 2: DMA tilemap to VRAM
state2_dma_tilemap:
    ; Wait for VBlank
    in a, (VDP_STATUS)
    and 0x01
    jr z, state2_dma_tilemap

    ; DMA tilemap: 1536 bytes from TILEMAP_DATA_BUF to VRAM 0x1000
    ; SRC = TILEMAP_DATA_BUF (0xC180)
    ld a, 0x80
    out (DMA_SRC_LO), a
    ld a, 0xC1
    out (DMA_SRC_HI), a

    ; DST = 0x1000 (VRAM tilemap base)
    ld a, 0x00
    out (DMA_DST_LO), a
    ld a, 0x10
    out (DMA_DST_HI), a

    ; LEN = 1536 (0x0600) - 32x24 entries * 2 bytes
    ld a, 0x00
    out (DMA_LEN_LO), a
    ld a, 0x06
    out (DMA_LEN_HI), a

    ; CTRL = START
    ld a, DMA_START
    out (DMA_CTRL), a

    ; Advance to state 3
    ld a, 3
    ld (TEST_STATE), a
    jp main_loop

; State 3: Setup PPU registers and enable display
state3_setup_ppu:
    ; Set pattern base = page 0
    ld a, PATTERN_PAGE
    out (PATTERN_BASE), a

    ; Set tilemap base = page 4
    ld a, TILEMAP_PAGE
    out (PLANE_A_BASE), a

    ; Set initial scroll to 0
    xor a
    out (SCROLL_X), a
    out (SCROLL_Y), a

    ; Enable display
    ld a, 0x01
    out (VDP_CTRL), a

    ; Advance to state 4
    ld a, 4
    ld (TEST_STATE), a
    jp main_loop

; State 4: Continuous scroll animation
state4_scroll_loop:
    ; Wait for VBlank
    in a, (VDP_STATUS)
    and 0x01
    jr z, state4_scroll_loop

    ; Increment scroll X
    ld a, (CURRENT_SCROLL_X)
    inc a
    ld (CURRENT_SCROLL_X), a
    out (SCROLL_X), a

    ; Wait until VBlank ends to avoid multiple updates per frame
state4_wait_active:
    in a, (VDP_STATUS)
    and 0x01
    jr nz, state4_wait_active

    ; Loop back
    jp state4_scroll_loop

; Generate 4 test tiles in TILE_DATA_BUF
; Each tile is 8x8 pixels, 4bpp packed = 32 bytes
generate_tiles:
    ld hl, TILE_DATA_BUF

    ; Tile 0: Solid color (palette index 15 = white)
    ; Each byte: 0xFF (both pixels = index 15)
    ld b, 32
    ld a, 0xFF
tile0_loop:
    ld (hl), a
    inc hl
    djnz tile0_loop

    ; Tile 1: Checkerboard (alternating 0 and 15)
    ; Row 0: 0F 0F 0F 0F (indices 0,15,0,15,0,15,0,15)
    ; Row 1: F0 F0 F0 F0 (indices 15,0,15,0,15,0,15,0)
    ld c, 4                 ; 4 pairs of rows
tile1_outer:
    ; Even row: 0F 0F 0F 0F
    ld b, 4
    ld a, 0x0F
tile1_even:
    ld (hl), a
    inc hl
    djnz tile1_even

    ; Odd row: F0 F0 F0 F0
    ld b, 4
    ld a, 0xF0
tile1_odd:
    ld (hl), a
    inc hl
    djnz tile1_odd

    dec c
    jr nz, tile1_outer

    ; Tile 2: Vertical stripes (columns alternate 0 and 12=red)
    ; Each byte: 0C (left=0, right=12)
    ld b, 32
    ld a, 0x0C              ; Palette 0 and 12
tile2_loop:
    ld (hl), a
    inc hl
    djnz tile2_loop

    ; Tile 3: Horizontal stripes (rows alternate 0 and 10=green)
    ; Even rows: all 0 (0x00)
    ; Odd rows: all 10 (0xAA)
    ld c, 4                 ; 4 pairs of rows
tile3_outer:
    ; Even row: 00 00 00 00
    ld b, 4
    xor a
tile3_even:
    ld (hl), a
    inc hl
    djnz tile3_even

    ; Odd row: AA AA AA AA (index 10 = 0xA)
    ld b, 4
    ld a, 0xAA
tile3_odd:
    ld (hl), a
    inc hl
    djnz tile3_odd

    dec c
    jr nz, tile3_outer

    ret

; Generate 32x24 tilemap in TILEMAP_DATA_BUF
; Each entry is 16-bit little-endian
; Pattern: repeating [0, 1, 2, 3, 0, 1, 2, 3, ...]
generate_tilemap:
    ld hl, TILEMAP_DATA_BUF
    ld de, 768              ; 32x24 = 768 entries
    ld c, 0                 ; Tile index counter (0-3)

tilemap_loop:
    ; Write 16-bit entry: tile_index (low byte), 0 (high byte)
    ld a, c
    ld (hl), a              ; Low byte = tile index
    inc hl
    xor a
    ld (hl), a              ; High byte = 0
    inc hl

    ; Increment tile index (0,1,2,3,0,1,2,3,...)
    ld a, c
    inc a
    and 0x03                ; Wrap at 4
    ld c, a

    ; Decrement entry count
    dec de
    ld a, d
    or e
    jr nz, tilemap_loop

    ret

; Padding to fill ROM
    org 0x03FF
    db 0x00
