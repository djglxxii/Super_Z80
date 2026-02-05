; Phase 10 Plane B Test ROM
; Super_Z80 Emulator Bring-Up
;
; Purpose: Validate dual-plane (Plane A + Plane B) rendering with parallax
;
; Test cases:
; 1. Load distinct patterns for Plane A (foreground) and Plane B (background)
; 2. Create two tilemaps at different VRAM pages
; 3. Enable both planes (VDP_CTRL bit 0=display, bit 1=Plane B)
; 4. Scroll Plane B slowly while Plane A remains static
; 5. Verify Plane A pixels (non-transparent) occlude Plane B
;
; Expected outcome:
; - Two distinct layers visible
; - Plane B scrolls smoothly behind Plane A
; - Where Plane A has non-zero palette index, Plane B is hidden
; - No tearing or corruption

; I/O Port Definitions
IRQ_STATUS  equ 0x80    ; R: IRQ pending bits
IRQ_ENABLE  equ 0x81    ; R/W: IRQ enable mask
IRQ_ACK     equ 0x82    ; W: write-1-to-clear pending bits
VDP_STATUS  equ 0x10    ; R: VDP status (bit 0 = VBlank)

; PPU Registers
VDP_CTRL        equ 0x11    ; R/W: bit 0 = display enable, bit 1 = Plane B enable
SCROLL_X        equ 0x12    ; R/W: Plane A scroll X (pixels)
SCROLL_Y        equ 0x13    ; R/W: Plane A scroll Y (pixels)
PLANE_B_SCROLL_X equ 0x14   ; R/W: Plane B scroll X (pixels) [Phase 10]
PLANE_B_SCROLL_Y equ 0x15   ; R/W: Plane B scroll Y (pixels) [Phase 10]
PLANE_A_BASE    equ 0x16    ; R/W: Plane A tilemap base (page index, *1024)
PLANE_B_BASE    equ 0x17    ; R/W: Plane B tilemap base (page index, *1024) [Phase 10]
PATTERN_BASE    equ 0x18    ; R/W: Pattern base (page index, *1024)

; Palette I/O Ports
PAL_ADDR        equ 0x1E    ; R/W: Palette byte address (0-255)
PAL_DATA        equ 0x1F    ; R/W: Palette data byte (auto-increment)

; DMA I/O Ports
DMA_SRC_LO  equ 0x30
DMA_SRC_HI  equ 0x31
DMA_DST_LO  equ 0x32
DMA_DST_HI  equ 0x33
DMA_LEN_LO  equ 0x34
DMA_LEN_HI  equ 0x35
DMA_CTRL    equ 0x36

; DMA_CTRL bits
DMA_START       equ 0x01
DMA_DST_PALETTE equ 0x08

; VRAM Layout (Phase 10):
;   Pattern base = page 0 (0x0000)  - 6 tiles = 192 bytes
;   Plane A tilemap = page 4 (0x1000)  - 32x24 entries = 1536 bytes
;   Plane B tilemap = page 8 (0x2000)  - 32x24 entries = 1536 bytes
VRAM_PATTERN_BASE   equ 0x0000
VRAM_TILEMAP_A_BASE equ 0x1000
VRAM_TILEMAP_B_BASE equ 0x2000
PATTERN_PAGE        equ 0
TILEMAP_A_PAGE      equ 4
TILEMAP_B_PAGE      equ 8

; RAM Locations
ISR_VBLANK_COUNT    equ 0xC000  ; u16: incremented in ISR
PLANE_B_X           equ 0xC002  ; u8: Plane B scroll X
PLANE_B_Y           equ 0xC003  ; u8: Plane B scroll Y
FRAME_COUNTER       equ 0xC004  ; u8: frame counter for slower scroll
TEST_STATE          equ 0xC005  ; u8: current test state

; Data buffers
TILE_DATA_BUF       equ 0xC100  ; 192 bytes: 6 tiles * 32 bytes each
TILEMAP_A_BUF       equ 0xC1C0  ; 1536 bytes: 32x24 * 2 bytes (ends at 0xC7C0)
TILEMAP_B_BUF       equ 0xC7C0  ; 1536 bytes: 32x24 * 2 bytes (ends at 0xCDC0)
PALETTE_DATA_BUF    equ 0xCDC0  ; 32 bytes: 16 palette entries

; Reset vector (0x0000)
    org 0x0000
reset:
    di
    ld sp, 0xFFFE

    ; Clear counters
    ld hl, ISR_VBLANK_COUNT
    ld (hl), 0x00
    inc hl
    ld (hl), 0x00

    ; Initialize scroll state
    xor a
    ld (PLANE_B_X), a
    ld (PLANE_B_Y), a
    ld (FRAME_COUNTER), a
    ld (TEST_STATE), a

    ; Generate tile patterns
    call generate_tiles

    ; Generate tilemaps (A and B)
    call generate_tilemap_a
    call generate_tilemap_b

    ; Generate palette
    call generate_palette

    ; Set interrupt mode 1
    im 1

    ; Enable VBlank IRQ
    ld a, 0x01
    out (IRQ_ENABLE), a

    ei
    jp main_loop

; ISR at 0x0038 (IM 1 vector)
    org 0x0038
isr_vblank:
    push af
    push hl

    ; Increment VBlank count
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

    ; Acknowledge VBlank
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
    jr z, state2_dma_tilemap_a
    cp 3
    jr z, state3_dma_tilemap_b
    cp 4
    jr z, state4_dma_palette
    cp 5
    jr z, state5_setup_ppu
    cp 6
    jr z, state6_parallax_loop
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

    ; DMA: 192 bytes from TILE_DATA_BUF to VRAM 0x0000
    ld a, LOW(TILE_DATA_BUF)
    out (DMA_SRC_LO), a
    ld a, HIGH(TILE_DATA_BUF)
    out (DMA_SRC_HI), a

    ld a, 0x00
    out (DMA_DST_LO), a
    ld a, 0x00
    out (DMA_DST_HI), a

    ld a, 0xC0              ; 192 bytes = 6 tiles
    out (DMA_LEN_LO), a
    ld a, 0x00
    out (DMA_LEN_HI), a

    ld a, DMA_START
    out (DMA_CTRL), a

    ld a, 2
    ld (TEST_STATE), a
    jp main_loop

; State 2: DMA Plane A tilemap to VRAM
state2_dma_tilemap_a:
    in a, (VDP_STATUS)
    and 0x01
    jr z, state2_dma_tilemap_a

    ; DMA: 1536 bytes from TILEMAP_A_BUF to VRAM 0x1000
    ld a, LOW(TILEMAP_A_BUF)
    out (DMA_SRC_LO), a
    ld a, HIGH(TILEMAP_A_BUF)
    out (DMA_SRC_HI), a

    ld a, 0x00
    out (DMA_DST_LO), a
    ld a, 0x10
    out (DMA_DST_HI), a

    ld a, 0x00              ; 1536 = 0x0600
    out (DMA_LEN_LO), a
    ld a, 0x06
    out (DMA_LEN_HI), a

    ld a, DMA_START
    out (DMA_CTRL), a

    ld a, 3
    ld (TEST_STATE), a
    jp main_loop

; State 3: DMA Plane B tilemap to VRAM
state3_dma_tilemap_b:
    in a, (VDP_STATUS)
    and 0x01
    jr z, state3_dma_tilemap_b

    ; DMA: 1536 bytes from TILEMAP_B_BUF to VRAM 0x2000
    ld a, LOW(TILEMAP_B_BUF)
    out (DMA_SRC_LO), a
    ld a, HIGH(TILEMAP_B_BUF)
    out (DMA_SRC_HI), a

    ld a, 0x00
    out (DMA_DST_LO), a
    ld a, 0x20
    out (DMA_DST_HI), a

    ld a, 0x00
    out (DMA_LEN_LO), a
    ld a, 0x06
    out (DMA_LEN_HI), a

    ld a, DMA_START
    out (DMA_CTRL), a

    ld a, 4
    ld (TEST_STATE), a
    jp main_loop

; State 4: DMA palette to Palette RAM
state4_dma_palette:
    in a, (VDP_STATUS)
    and 0x01
    jr z, state4_dma_palette

    ld a, LOW(PALETTE_DATA_BUF)
    out (DMA_SRC_LO), a
    ld a, HIGH(PALETTE_DATA_BUF)
    out (DMA_SRC_HI), a

    ld a, 0x00
    out (DMA_DST_LO), a
    ld a, 0x00
    out (DMA_DST_HI), a

    ld a, 0x20              ; 32 bytes = 16 entries
    out (DMA_LEN_LO), a
    ld a, 0x00
    out (DMA_LEN_HI), a

    ld a, DMA_START | DMA_DST_PALETTE
    out (DMA_CTRL), a

    ld a, 5
    ld (TEST_STATE), a
    jp main_loop

; State 5: Setup PPU and enable both planes
state5_setup_ppu:
    ; Set pattern base = page 0
    ld a, PATTERN_PAGE
    out (PATTERN_BASE), a

    ; Set Plane A tilemap base = page 4
    ld a, TILEMAP_A_PAGE
    out (PLANE_A_BASE), a

    ; Set Plane B tilemap base = page 8 [Phase 10]
    ld a, TILEMAP_B_PAGE
    out (PLANE_B_BASE), a

    ; Plane A scroll = (0, 0) - static foreground
    xor a
    out (SCROLL_X), a
    out (SCROLL_Y), a

    ; Plane B scroll = (0, 0) initially
    out (PLANE_B_SCROLL_X), a
    out (PLANE_B_SCROLL_Y), a

    ; Enable display + Plane B: VDP_CTRL = 0x03 (bit 0 + bit 1)
    ld a, 0x03
    out (VDP_CTRL), a

    ld a, 6
    ld (TEST_STATE), a
    jp main_loop

; State 6: Parallax scroll loop
state6_parallax_loop:
    ; Wait for VBlank
    in a, (VDP_STATUS)
    and 0x01
    jr z, state6_parallax_loop

    ; Increment frame counter (slow down Plane B scroll)
    ld a, (FRAME_COUNTER)
    inc a
    ld (FRAME_COUNTER), a

    ; Only update scroll every 2 frames for smoother parallax effect
    and 0x01
    jr nz, state6_skip_scroll

    ; Increment Plane B scroll X (horizontal parallax)
    ld a, (PLANE_B_X)
    inc a
    ld (PLANE_B_X), a
    out (PLANE_B_SCROLL_X), a

    ; Slowly increment Plane B scroll Y (diagonal scroll for visibility)
    ld a, (FRAME_COUNTER)
    and 0x07              ; Every 8 frames
    jr nz, state6_skip_scroll

    ld a, (PLANE_B_Y)
    inc a
    ld (PLANE_B_Y), a
    out (PLANE_B_SCROLL_Y), a

state6_skip_scroll:
    ; Wait for VBlank to end
state6_wait_active:
    in a, (VDP_STATUS)
    and 0x01
    jr nz, state6_wait_active

    jp state6_parallax_loop

; Generate 6 test tiles in TILE_DATA_BUF
; Tile 0: Transparent (all 0s - used for Plane A background)
; Tile 1: Solid block (palette 1 - foreground solid)
; Tile 2: Border frame (palette 2 - foreground frame)
; Tile 3: Checkerboard (palette 3+4 - background pattern 1)
; Tile 4: Diagonal stripes (palette 5 - background pattern 2)
; Tile 5: Cross hatch (palette 6 - background pattern 3)
generate_tiles:
    ld hl, TILE_DATA_BUF

    ; Tile 0: Fully transparent (all 0)
    ld b, 32
    xor a
tile0_loop:
    ld (hl), a
    inc hl
    djnz tile0_loop

    ; Tile 1: Solid block (palette index 1 = 0x11)
    ld b, 32
    ld a, 0x11
tile1_loop:
    ld (hl), a
    inc hl
    djnz tile1_loop

    ; Tile 2: Border frame (edges are palette 2, center is transparent)
    ; Row 0: 22 22 22 22 (all index 2)
    ld b, 4
    ld a, 0x22
tile2_row0:
    ld (hl), a
    inc hl
    djnz tile2_row0

    ; Rows 1-6: edges only (20 00 00 02)
    ld c, 6
tile2_middle:
    ld (hl), 0x20           ; Left edge
    inc hl
    ld (hl), 0x00           ; Center
    inc hl
    ld (hl), 0x00           ; Center
    inc hl
    ld (hl), 0x02           ; Right edge
    inc hl
    dec c
    jr nz, tile2_middle

    ; Row 7: 22 22 22 22 (all index 2)
    ld b, 4
    ld a, 0x22
tile2_row7:
    ld (hl), a
    inc hl
    djnz tile2_row7

    ; Tile 3: Checkerboard (palette 3 and 4)
    ld c, 4                 ; 4 row pairs
tile3_outer:
    ; Even row: 34 34 34 34
    ld b, 4
    ld a, 0x34
tile3_even:
    ld (hl), a
    inc hl
    djnz tile3_even

    ; Odd row: 43 43 43 43
    ld b, 4
    ld a, 0x43
tile3_odd:
    ld (hl), a
    inc hl
    djnz tile3_odd

    dec c
    jr nz, tile3_outer

    ; Tile 4: Diagonal stripes (palette 5)
    ; Pattern shifts left each row
    ld a, 0x50              ; Start pattern
    ld c, 8                 ; 8 rows
tile4_loop:
    ld b, 4
tile4_row:
    ld (hl), a
    inc hl
    djnz tile4_row

    ; Rotate pattern for next row
    rlca
    rlca                    ; Shift pattern
    dec c
    jr nz, tile4_loop

    ; Tile 5: Cross hatch (palette 6)
    ; Alternating pattern
    ld c, 4
tile5_outer:
    ; Even row: 66 00 66 00
    ld (hl), 0x66
    inc hl
    ld (hl), 0x00
    inc hl
    ld (hl), 0x66
    inc hl
    ld (hl), 0x00
    inc hl

    ; Odd row: 00 66 00 66
    ld (hl), 0x00
    inc hl
    ld (hl), 0x66
    inc hl
    ld (hl), 0x00
    inc hl
    ld (hl), 0x66
    inc hl

    dec c
    jr nz, tile5_outer

    ret

; Generate Plane A tilemap (foreground)
; Pattern: mostly transparent (tile 0) with scattered solid blocks (tile 1)
; and border frames (tile 2) - creates windows to see Plane B behind
generate_tilemap_a:
    ld hl, TILEMAP_A_BUF
    ld de, 768              ; 32x24 entries
    ld c, 0                 ; Position counter

tilemap_a_loop:
    ; Create a pattern: sparse foreground elements
    ; Every 5th column and 4th row has a solid block
    ; Every 7th has a border frame
    ld a, c
    and 0x1F                ; Column (0-31)
    ld b, a

    ; Check if this should be a solid block
    ld a, c
    and 0x04                ; Every 4 entries in X
    jr nz, ta_check_frame

    ; Check Y position (c / 32)
    ld a, c
    srl a
    srl a
    srl a
    srl a
    srl a                   ; c / 32 = row
    and 0x03                ; Every 4 rows
    jr nz, ta_check_frame

    ; Place solid block (tile 1)
    ld (hl), 0x01
    inc hl
    ld (hl), 0x00
    inc hl
    jr ta_next

ta_check_frame:
    ; Check for border frame placement
    ld a, c
    and 0x07                ; Every 8 entries
    cp 3
    jr nz, ta_transparent

    ; Check row
    ld a, c
    srl a
    srl a
    srl a
    srl a
    srl a
    and 0x07                ; Row mod 8
    cp 2
    jr nz, ta_transparent

    ; Place border frame (tile 2)
    ld (hl), 0x02
    inc hl
    ld (hl), 0x00
    inc hl
    jr ta_next

ta_transparent:
    ; Transparent (tile 0)
    ld (hl), 0x00
    inc hl
    ld (hl), 0x00
    inc hl

ta_next:
    inc c
    dec de
    ld a, d
    or e
    jr nz, tilemap_a_loop

    ret

; Generate Plane B tilemap (background)
; Pattern: repeating background tiles (3, 4, 5)
generate_tilemap_b:
    ld hl, TILEMAP_B_BUF
    ld de, 768              ; 32x24 entries
    ld c, 3                 ; Start with tile 3

tilemap_b_loop:
    ; Write tile index
    ld a, c
    ld (hl), a
    inc hl
    xor a
    ld (hl), a
    inc hl

    ; Cycle through tiles 3, 4, 5
    ld a, c
    inc a
    cp 6
    jr c, tb_no_wrap
    ld a, 3
tb_no_wrap:
    ld c, a

    dec de
    ld a, d
    or e
    jr nz, tilemap_b_loop

    ret

; Generate palette
; 9-bit RGB: bits 0-2=R, 3-5=G, 6-8=B
generate_palette:
    ld hl, PALETTE_DATA_BUF

    ; Entry 0: Transparent/Black (R=0, G=0, B=0) -> 0x0000
    ld (hl), 0x00
    inc hl
    ld (hl), 0x00
    inc hl

    ; Entry 1: White (R=7, G=7, B=7) -> 0x01FF (foreground solid)
    ld (hl), 0xFF
    inc hl
    ld (hl), 0x01
    inc hl

    ; Entry 2: Yellow (R=7, G=7, B=0) -> 0x003F (foreground frame)
    ld (hl), 0x3F
    inc hl
    ld (hl), 0x00
    inc hl

    ; Entry 3: Dark Red (R=4, G=0, B=0) -> 0x0004 (background 1)
    ld (hl), 0x04
    inc hl
    ld (hl), 0x00
    inc hl

    ; Entry 4: Dark Cyan (R=0, G=4, B=4) -> 0x0120 (background 1 alt)
    ld (hl), 0x20
    inc hl
    ld (hl), 0x01
    inc hl

    ; Entry 5: Dark Green (R=0, G=4, B=0) -> 0x0020 (background 2)
    ld (hl), 0x20
    inc hl
    ld (hl), 0x00
    inc hl

    ; Entry 6: Dark Blue (R=0, G=0, B=4) -> 0x0100 (background 3)
    ld (hl), 0x00
    inc hl
    ld (hl), 0x01
    inc hl

    ; Entries 7-15: Black (fill remaining)
    ld b, 18                ; 9 entries * 2 bytes
    xor a
fill_pal:
    ld (hl), a
    inc hl
    djnz fill_pal

    ret

; Padding
    org 0x07FF
    db 0x00
