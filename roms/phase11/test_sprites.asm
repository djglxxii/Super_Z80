; Phase 11 Sprite Test ROM
; Super_Z80 Emulator Bring-Up
;
; Purpose: Validate sprite system functionality
;
; Test cases:
; 1. Load sprite tile patterns to VRAM
; 2. Configure SAT with multiple sprites at various positions
; 3. Test sprite overflow (>16 sprites on scanline 80)
; 4. Test behind-plane-A priority flag
; 5. Animate a moving sprite to verify smooth rendering
;
; Expected outcome:
; - Sprites render correctly at specified positions
; - Only first 16 sprites appear on scanline 80, overflow flag set
; - Behind-A sprites are occluded by Plane A pixels
; - Moving sprite animates smoothly without flicker

; I/O Port Definitions
IRQ_STATUS  equ 0x80    ; R: IRQ pending bits
IRQ_ENABLE  equ 0x81    ; R/W: IRQ enable mask
IRQ_ACK     equ 0x82    ; W: write-1-to-clear pending bits
VDP_STATUS  equ 0x10    ; R: VDP status (bit 0 = VBlank)

; PPU Registers
VDP_CTRL        equ 0x11    ; R/W: bit 0 = display enable, bit 1 = Plane B enable
SCROLL_X        equ 0x12    ; R/W: Plane A scroll X (pixels)
SCROLL_Y        equ 0x13    ; R/W: Plane A scroll Y (pixels)
PLANE_B_SCROLL_X equ 0x14   ; R/W: Plane B scroll X (pixels)
PLANE_B_SCROLL_Y equ 0x15   ; R/W: Plane B scroll Y (pixels)
PLANE_A_BASE    equ 0x16    ; R/W: Plane A tilemap base (page index, *1024)
PLANE_B_BASE    equ 0x17    ; R/W: Plane B tilemap base (page index, *1024)
PATTERN_BASE    equ 0x18    ; R/W: Pattern base (page index, *1024)

; Palette I/O Ports
PAL_ADDR        equ 0x1E    ; R/W: Palette byte address (0-255)
PAL_DATA        equ 0x1F    ; R/W: Palette data byte (auto-increment)

; Sprite I/O Ports (Phase 11)
SPR_CTRL        equ 0x20    ; R/W: bit 0 = sprite enable, bits 1-2 = size mode
SAT_BASE        equ 0x21    ; R/W: SAT base (page index * 256)
SPR_STATUS      equ 0x22    ; R: bit 0 = overflow latch

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

; VRAM Layout:
;   Pattern base = page 0 (0x0000) - tiles for sprites and backgrounds
;   Plane A tilemap = page 4 (0x1000)
;   SAT = page 24 (0x3000) - 48 entries * 8 bytes = 384 bytes
VRAM_PATTERN_BASE   equ 0x0000
VRAM_TILEMAP_A_BASE equ 0x1000
VRAM_SAT_BASE       equ 0x3000
PATTERN_PAGE        equ 0
TILEMAP_A_PAGE      equ 4
SAT_PAGE            equ 48      ; 0x3000 / 256 = 48

; RAM Locations
ISR_VBLANK_COUNT    equ 0xC000  ; u16: incremented in ISR
MOVING_SPRITE_X     equ 0xC002  ; u8: X position of moving sprite
FRAME_COUNTER       equ 0xC003  ; u8: frame counter
TEST_STATE          equ 0xC004  ; u8: current test state
LAST_OVERFLOW       equ 0xC005  ; u8: last overflow status read

; Data buffers
TILE_DATA_BUF       equ 0xC100  ; 256 bytes: 8 tiles * 32 bytes each
TILEMAP_A_BUF       equ 0xC200  ; 1536 bytes: 32x24 * 2 bytes
SAT_DATA_BUF        equ 0xC800  ; 384 bytes: 48 sprites * 8 bytes
PALETTE_DATA_BUF    equ 0xC980  ; 64 bytes: 32 palette entries

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

    ; Initialize state
    xor a
    ld (MOVING_SPRITE_X), a
    ld (FRAME_COUNTER), a
    ld (TEST_STATE), a
    ld (LAST_OVERFLOW), a

    ; Generate data
    call generate_tiles
    call generate_tilemap_a
    call generate_sat
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
    jr z, state2_dma_tilemap
    cp 3
    jr z, state3_dma_sat
    cp 4
    jr z, state4_dma_palette
    cp 5
    jr z, state5_setup_ppu
    cp 6
    jp z, state6_animation_loop
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

    ; DMA: 256 bytes from TILE_DATA_BUF to VRAM 0x0000
    ld a, LOW(TILE_DATA_BUF)
    out (DMA_SRC_LO), a
    ld a, HIGH(TILE_DATA_BUF)
    out (DMA_SRC_HI), a

    ld a, 0x00
    out (DMA_DST_LO), a
    ld a, 0x00
    out (DMA_DST_HI), a

    ld a, 0x00              ; 256 bytes = 8 tiles
    out (DMA_LEN_LO), a
    ld a, 0x01
    out (DMA_LEN_HI), a

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

; State 3: DMA SAT to VRAM
state3_dma_sat:
    in a, (VDP_STATUS)
    and 0x01
    jr z, state3_dma_sat

    ; DMA: 384 bytes from SAT_DATA_BUF to VRAM 0x3000
    ld a, LOW(SAT_DATA_BUF)
    out (DMA_SRC_LO), a
    ld a, HIGH(SAT_DATA_BUF)
    out (DMA_SRC_HI), a

    ld a, 0x00
    out (DMA_DST_LO), a
    ld a, 0x30
    out (DMA_DST_HI), a

    ld a, 0x80              ; 384 = 0x0180
    out (DMA_LEN_LO), a
    ld a, 0x01
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

    ld a, 0x40              ; 64 bytes = 32 entries
    out (DMA_LEN_LO), a
    ld a, 0x00
    out (DMA_LEN_HI), a

    ld a, DMA_START | DMA_DST_PALETTE
    out (DMA_CTRL), a

    ld a, 5
    ld (TEST_STATE), a
    jp main_loop

; State 5: Setup PPU and enable sprites
state5_setup_ppu:
    ; Set pattern base = page 0
    ld a, PATTERN_PAGE
    out (PATTERN_BASE), a

    ; Set Plane A tilemap base = page 4
    ld a, TILEMAP_A_PAGE
    out (PLANE_A_BASE), a

    ; Plane A scroll = (0, 0)
    xor a
    out (SCROLL_X), a
    out (SCROLL_Y), a

    ; Enable display (bit 0), no Plane B for simplicity
    ld a, 0x01
    out (VDP_CTRL), a

    ; Setup sprites
    ; SAT base = page 48 (0x3000)
    ld a, SAT_PAGE
    out (SAT_BASE), a

    ; Enable sprites (SPR_CTRL bit 0 = 1, size mode 00 = 8x8)
    ld a, 0x01
    out (SPR_CTRL), a

    ld a, 6
    ld (TEST_STATE), a
    jp main_loop

; State 6: Animation loop - move sprite and read overflow
state6_animation_loop:
    ; Wait for VBlank
    in a, (VDP_STATUS)
    and 0x01
    jr z, state6_animation_loop

    ; Read overflow status (for debug observation)
    in a, (SPR_STATUS)
    ld (LAST_OVERFLOW), a

    ; Update moving sprite X position
    ld a, (MOVING_SPRITE_X)
    inc a
    ld (MOVING_SPRITE_X), a

    ; Update sprite 0's X position in SAT buffer
    ld hl, SAT_DATA_BUF + 1   ; Sprite 0, byte 1 = X position
    ld (hl), a

    ; Re-DMA the first SAT entry (8 bytes) to update VRAM
    ld a, LOW(SAT_DATA_BUF)
    out (DMA_SRC_LO), a
    ld a, HIGH(SAT_DATA_BUF)
    out (DMA_SRC_HI), a

    ld a, 0x00
    out (DMA_DST_LO), a
    ld a, 0x30
    out (DMA_DST_HI), a

    ld a, 0x08              ; 8 bytes = 1 sprite entry
    out (DMA_LEN_LO), a
    ld a, 0x00
    out (DMA_LEN_HI), a

    ld a, DMA_START
    out (DMA_CTRL), a

    ; Wait for VBlank to end
state6_wait_active:
    in a, (VDP_STATUS)
    and 0x01
    jr nz, state6_wait_active

    jp state6_animation_loop

; Generate 8 test tiles in TILE_DATA_BUF
; Tile 0: Solid white (for background)
; Tile 1: Red sprite pattern
; Tile 2: Green sprite pattern
; Tile 3: Blue sprite pattern
; Tile 4: Yellow sprite pattern (for behind-A test)
; Tile 5: Checkerboard (Plane A foreground)
; Tile 6-7: Reserved
generate_tiles:
    ld hl, TILE_DATA_BUF

    ; Tile 0: Solid white (palette index 1)
    ld b, 32
    ld a, 0x11
tile0_loop:
    ld (hl), a
    inc hl
    djnz tile0_loop

    ; Tile 1: Solid red sprite (palette index 2)
    ld b, 32
    ld a, 0x22
tile1_loop:
    ld (hl), a
    inc hl
    djnz tile1_loop

    ; Tile 2: Solid green sprite (palette index 3)
    ld b, 32
    ld a, 0x33
tile2_loop:
    ld (hl), a
    inc hl
    djnz tile2_loop

    ; Tile 3: Solid blue sprite (palette index 4)
    ld b, 32
    ld a, 0x44
tile3_loop:
    ld (hl), a
    inc hl
    djnz tile3_loop

    ; Tile 4: Solid yellow sprite (palette index 5) - for behind test
    ld b, 32
    ld a, 0x55
tile4_loop:
    ld (hl), a
    inc hl
    djnz tile4_loop

    ; Tile 5: Checkerboard (palette 6 and 0) - foreground with holes
    ld c, 4                 ; 4 row pairs
tile5_outer:
    ; Even row: 60 60 60 60
    ld b, 4
    ld a, 0x60
tile5_even:
    ld (hl), a
    inc hl
    djnz tile5_even

    ; Odd row: 06 06 06 06
    ld b, 4
    ld a, 0x06
tile5_odd:
    ld (hl), a
    inc hl
    djnz tile5_odd

    dec c
    jr nz, tile5_outer

    ; Tiles 6-7: Fill with zeros
    ld b, 64
    xor a
tile67_loop:
    ld (hl), a
    inc hl
    djnz tile67_loop

    ret

; Generate Plane A tilemap
; Mostly tile 0 (white background) with some tile 5 (checkerboard) for
; testing sprite behind-plane priority
generate_tilemap_a:
    ld hl, TILEMAP_A_BUF
    ld de, 768              ; 32x24 entries
    ld c, 0                 ; Position counter

tilemap_a_loop:
    ; Put checkerboard tiles in a strip around Y=80 (scanline 80)
    ; to test behind-plane-A sprites
    ld a, c
    srl a
    srl a
    srl a
    srl a
    srl a                   ; c / 32 = row (0-23)
    cp 10                   ; Row 10 = Y pixels 80-87
    jr nz, ta_white

    ; Row 10: checkerboard (tile 5)
    ld (hl), 0x05
    inc hl
    ld (hl), 0x00
    inc hl
    jr ta_next

ta_white:
    ; Default: white (tile 0)
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

; Generate SAT data
; Create sprites to test:
; 1. Sprite 0: Moving red sprite at Y=40
; 2. Sprites 1-20: 20 sprites at Y=80 to test overflow (only 16 should render)
; 3. Sprite 21: Green sprite with behind-A flag at Y=80 (should be occluded by tile 5)
; 4. Sprite 22: Blue sprite (normal priority) at Y=120
; 5-47: Unused (Y=0 to effectively disable)
generate_sat:
    ld hl, SAT_DATA_BUF

    ; --- Sprite 0: Moving red sprite ---
    ; Y=40, X=0 (will animate), Tile=1 (red), Attr=0
    ld (hl), 40             ; Byte 0: Y
    inc hl
    ld (hl), 0              ; Byte 1: X (animated)
    inc hl
    ld (hl), 0x01           ; Byte 2: Tile LO
    inc hl
    ld (hl), 0x00           ; Byte 3: Tile HI
    inc hl
    ld (hl), 0x00           ; Byte 4: Attr (palette 0, normal priority)
    inc hl
    ld (hl), 0x00           ; Byte 5: Reserved
    inc hl
    ld (hl), 0x00           ; Byte 6: Reserved
    inc hl
    ld (hl), 0x00           ; Byte 7: Reserved
    inc hl

    ; --- Sprites 1-20: Overflow test at Y=80 ---
    ; Place 20 sprites all at Y=80 with different X positions
    ; Only first 16 should render (indices 1-16)
    ld c, 20                ; 20 sprites
    ld b, 0                 ; X position starts at 0
sat_overflow_loop:
    ld (hl), 80             ; Byte 0: Y
    inc hl
    ld a, b
    ld (hl), a              ; Byte 1: X
    inc hl
    ld (hl), 0x02           ; Byte 2: Tile LO (green)
    inc hl
    ld (hl), 0x00           ; Byte 3: Tile HI
    inc hl
    ld (hl), 0x00           ; Byte 4: Attr
    inc hl
    ld (hl), 0x00           ; Byte 5: Reserved
    inc hl
    ld (hl), 0x00           ; Byte 6: Reserved
    inc hl
    ld (hl), 0x00           ; Byte 7: Reserved
    inc hl

    ; Increment X by 12 (8 width + 4 gap)
    ld a, b
    add a, 12
    ld b, a

    dec c
    jr nz, sat_overflow_loop

    ; --- Sprite 21: Behind-A test (yellow) ---
    ; Y=80, X=200, Tile=4 (yellow), Attr=0x10 (behindPlaneA=1)
    ld (hl), 80             ; Byte 0: Y
    inc hl
    ld (hl), 200            ; Byte 1: X
    inc hl
    ld (hl), 0x04           ; Byte 2: Tile LO (yellow)
    inc hl
    ld (hl), 0x00           ; Byte 3: Tile HI
    inc hl
    ld (hl), 0x10           ; Byte 4: Attr = behind Plane A
    inc hl
    ld (hl), 0x00           ; Byte 5: Reserved
    inc hl
    ld (hl), 0x00           ; Byte 6: Reserved
    inc hl
    ld (hl), 0x00           ; Byte 7: Reserved
    inc hl

    ; --- Sprite 22: Normal blue sprite at Y=120 ---
    ld (hl), 120            ; Byte 0: Y
    inc hl
    ld (hl), 100            ; Byte 1: X
    inc hl
    ld (hl), 0x03           ; Byte 2: Tile LO (blue)
    inc hl
    ld (hl), 0x00           ; Byte 3: Tile HI
    inc hl
    ld (hl), 0x00           ; Byte 4: Attr
    inc hl
    ld (hl), 0x00           ; Byte 5: Reserved
    inc hl
    ld (hl), 0x00           ; Byte 6: Reserved
    inc hl
    ld (hl), 0x00           ; Byte 7: Reserved
    inc hl

    ; --- Sprites 23-47: Disabled (Y=0, won't appear on visible scanlines) ---
    ld c, 25                ; 25 remaining sprites
sat_disabled_loop:
    ld (hl), 0              ; Byte 0: Y=0 (off-screen)
    inc hl
    ld (hl), 0              ; Byte 1: X
    inc hl
    ld (hl), 0x00           ; Byte 2: Tile LO
    inc hl
    ld (hl), 0x00           ; Byte 3: Tile HI
    inc hl
    ld (hl), 0x00           ; Byte 4: Attr
    inc hl
    ld (hl), 0x00           ; Byte 5: Reserved
    inc hl
    ld (hl), 0x00           ; Byte 6: Reserved
    inc hl
    ld (hl), 0x00           ; Byte 7: Reserved
    inc hl
    dec c
    jr nz, sat_disabled_loop

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

    ; Entry 1: White (R=7, G=7, B=7) -> 0x01FF (background)
    ld (hl), 0xFF
    inc hl
    ld (hl), 0x01
    inc hl

    ; Entry 2: Red (R=7, G=0, B=0) -> 0x0007 (sprite 0)
    ld (hl), 0x07
    inc hl
    ld (hl), 0x00
    inc hl

    ; Entry 3: Green (R=0, G=7, B=0) -> 0x0038 (overflow test)
    ld (hl), 0x38
    inc hl
    ld (hl), 0x00
    inc hl

    ; Entry 4: Blue (R=0, G=0, B=7) -> 0x01C0 (normal sprite)
    ld (hl), 0xC0
    inc hl
    ld (hl), 0x01
    inc hl

    ; Entry 5: Yellow (R=7, G=7, B=0) -> 0x003F (behind-A test)
    ld (hl), 0x3F
    inc hl
    ld (hl), 0x00
    inc hl

    ; Entry 6: Magenta (R=7, G=0, B=7) -> 0x01C7 (checkerboard)
    ld (hl), 0xC7
    inc hl
    ld (hl), 0x01
    inc hl

    ; Entries 7-31: Black (fill remaining)
    ld b, 50                ; 25 entries * 2 bytes
    xor a
fill_pal:
    ld (hl), a
    inc hl
    djnz fill_pal

    ret

; Padding
    org 0x07FF
    db 0x00
