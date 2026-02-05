; ==================================================================
; Super_Z80 Diagnostic Cartridge ROM
; diag_cart.asm - Main ROM source
;
; Entry point: 0x0000
; ISR vector:  0x0038 (IM 1)
; ROM size:    <= 32KB
; ==================================================================

    device  NOSLOT64K       ; sjasmplus: 64K address space, no slot system
    org     0x0000          ; Entry point at reset vector

    include "hw.inc"        ; Hardware constants

; ==================================================================
; RESET VECTOR (0x0000)
; ==================================================================
reset:
    di                      ; Disable interrupts
    ld      sp, 0xFFFE      ; Initialize stack pointer
    jp      init            ; Jump to initialization

; ==================================================================
; PADDING TO ISR VECTOR
; ==================================================================
    ; Fill space between reset and ISR vector
    ds      0x0038 - $, 0x00

; ==================================================================
; IM 1 ISR VECTOR (0x0038)
; ==================================================================
    org     0x0038
isr_vblank:
    push    af
    push    hl
    push    bc

    ; 1. Increment ISR_ENTRY_COUNT (16-bit)
    ld      hl, (ISR_ENTRY_COUNT)
    inc     hl
    ld      (ISR_ENTRY_COUNT), hl

    ; 2. Increment VBLANK_COUNT (16-bit)
    ld      hl, (VBLANK_COUNT)
    inc     hl
    ld      (VBLANK_COUNT), hl

    ; 3. Toggle palette entry 1 between Red <-> Blue
    ; Read current value of SCRATCH to determine current color
    ld      a, (SCRATCH)
    xor     1               ; Toggle bit 0
    ld      (SCRATCH), a
    and     1               ; Isolate toggle bit
    jr      z, .set_red

.set_blue:
    ; Set palette entry 1 to Blue (0x01C0)
    ld      a, 2            ; Palette address = entry 1 * 2 = 2
    out     (PAL_ADDR), a
    ld      a, 0xC0         ; Low byte of 0x01C0
    out     (PAL_DATA), a
    ld      a, 0x01         ; High byte of 0x01C0
    out     (PAL_DATA), a
    jr      .ack_irq

.set_red:
    ; Set palette entry 1 to Red (0x0007)
    ld      a, 2            ; Palette address = entry 1 * 2 = 2
    out     (PAL_ADDR), a
    ld      a, 0x07         ; Low byte of 0x0007
    out     (PAL_DATA), a
    ld      a, 0x00         ; High byte of 0x0007
    out     (PAL_DATA), a

.ack_irq:
    ; 4. ACK VBlank via IRQ_ACK (write bit 0 = 1)
    ld      a, IRQ_VBLANK
    out     (IRQ_ACK), a

    pop     bc
    pop     hl
    pop     af
    ; 5. RETI
    ei                      ; Re-enable interrupts before return
    reti

; ==================================================================
; INITIALIZATION
; ==================================================================
init:
    ; Clear RAM region 0xC000-0xC3FF (1KB)
    ld      hl, 0xC000
    ld      de, 0xC001
    ld      bc, 0x03FF      ; 1023 bytes (first byte set, then copy)
    xor     a
    ld      (hl), a         ; Set first byte to 0
    ldir                    ; Copy 0 to rest of region

    ; Initialize video control
    call    init_video

    ; Copy palette data to RAM buffer and DMA to Palette RAM
    call    init_palette

    ; Copy tile patterns to RAM buffer and DMA to VRAM
    call    init_tiles

    ; Copy tilemap to RAM buffer and DMA to VRAM
    call    init_tilemap

    ; Set IM 1 (interrupt mode 1 - ISR at 0x0038)
    im      1

    ; Enable VBlank IRQ
    ld      a, IRQ_VBLANK
    out     (IRQ_ENABLE), a

    ; Enable interrupts (after all hardware setup)
    ei

    ; Fall through to main loop

; ==================================================================
; MAIN LOOP (runs forever, no HALT)
; ==================================================================
main_loop:
    ; Optionally increment FRAME_COUNTER by polling VBlank
    ; Wait for VBlank start (bit 0 of VDP_STATUS = 1)
.wait_vblank:
    in      a, (VDP_STATUS)
    and     0x01
    jr      z, .wait_vblank

    ; Increment frame counter
    ld      hl, (FRAME_COUNTER)
    inc     hl
    ld      (FRAME_COUNTER), hl

    ; Wait for VBlank end (bit 0 of VDP_STATUS = 0)
.wait_vblank_end:
    in      a, (VDP_STATUS)
    and     0x01
    jr      nz, .wait_vblank_end

    ; Loop forever
    jr      main_loop

; ==================================================================
; INIT_VIDEO - Initialize video control registers
; ==================================================================
init_video:
    ; Set VDP_CTRL: Display enable, Plane A enable, Plane B/Sprites disabled
    ; VDP_DISPLAY_EN (0x01) | VDP_PLANE_A_EN (0x02) = 0x03
    ld      a, VDP_DISPLAY_EN | VDP_PLANE_A_EN
    out     (VDP_CTRL), a

    ; Set scroll to 0,0
    xor     a
    out     (SCROLL_X), a
    out     (SCROLL_Y), a

    ; Set base page selectors
    ld      a, PATTERN_PAGE ; Page 0 for patterns
    out     (PATTERN_BASE), a

    ld      a, TILEMAP_PAGE ; Page 1 for tilemap
    out     (PLANE_A_BASE), a

    ret

; ==================================================================
; INIT_PALETTE - Load palette data and DMA to Palette RAM
; ==================================================================
init_palette:
    ; Copy palette data from ROM to RAM buffer at DMA_PAL_BUF (0xC100)
    ld      hl, palette_data
    ld      de, DMA_PAL_BUF
    ld      bc, PALETTE_DATA_SIZE
    ldir

    ; Wait for VBlank before DMA
    call    wait_vblank

    ; Set up DMA: RAM buffer -> Palette RAM
    ; Source: DMA_PAL_BUF (0xC100)
    ld      a, DMA_PAL_BUF & 0xFF
    out     (DMA_SRC_LO), a
    ld      a, DMA_PAL_BUF >> 8
    out     (DMA_SRC_HI), a

    ; Destination: Palette byte 0
    xor     a
    out     (DMA_DST_LO), a
    out     (DMA_DST_HI), a

    ; Length: PALETTE_DATA_SIZE (32 bytes)
    ld      a, PALETTE_DATA_SIZE & 0xFF
    out     (DMA_LEN_LO), a
    ld      a, PALETTE_DATA_SIZE >> 8
    out     (DMA_LEN_HI), a

    ; Start DMA to Palette RAM (DST_IS_PALETTE bit set)
    ld      a, DMA_START | DMA_PAL_DST
    out     (DMA_CTRL), a

    ret

; ==================================================================
; INIT_TILES - Load tile patterns and DMA to VRAM
; ==================================================================
init_tiles:
    ; Copy tile data from ROM to RAM buffer at DMA_TILE_BUF (0xC120)
    ld      hl, tile_0_data
    ld      de, DMA_TILE_BUF
    ld      bc, TILE_DATA_SIZE
    ldir

    ; Wait for VBlank before DMA
    call    wait_vblank

    ; Set up DMA: RAM buffer -> VRAM (pattern base)
    ; Source: DMA_TILE_BUF (0xC120)
    ld      a, DMA_TILE_BUF & 0xFF
    out     (DMA_SRC_LO), a
    ld      a, DMA_TILE_BUF >> 8
    out     (DMA_SRC_HI), a

    ; Destination: VRAM pattern area (PATTERN_VRAM = 0x0000)
    ld      a, PATTERN_VRAM & 0xFF
    out     (DMA_DST_LO), a
    ld      a, PATTERN_VRAM >> 8
    out     (DMA_DST_HI), a

    ; Length: TILE_DATA_SIZE (128 bytes)
    ld      a, TILE_DATA_SIZE & 0xFF
    out     (DMA_LEN_LO), a
    ld      a, TILE_DATA_SIZE >> 8
    out     (DMA_LEN_HI), a

    ; Start DMA to VRAM (DST_IS_PALETTE bit clear)
    ld      a, DMA_START
    out     (DMA_CTRL), a

    ret

; ==================================================================
; INIT_TILEMAP - Load tilemap and DMA to VRAM
; ==================================================================
init_tilemap:
    ; Copy tilemap data from ROM to RAM buffer at TILEMAP_BUF (0xC200)
    ld      hl, tilemap_data
    ld      de, TILEMAP_BUF
    ld      bc, TILEMAP_DATA_SIZE
    ldir

    ; Wait for VBlank before DMA
    call    wait_vblank

    ; Set up DMA: RAM buffer -> VRAM (tilemap base)
    ; Source: TILEMAP_BUF (0xC200)
    ld      a, TILEMAP_BUF & 0xFF
    out     (DMA_SRC_LO), a
    ld      a, TILEMAP_BUF >> 8
    out     (DMA_SRC_HI), a

    ; Destination: VRAM tilemap area (TILEMAP_VRAM = 0x0800)
    ld      a, TILEMAP_VRAM & 0xFF
    out     (DMA_DST_LO), a
    ld      a, TILEMAP_VRAM >> 8
    out     (DMA_DST_HI), a

    ; Length: TILEMAP_DATA_SIZE (1536 bytes)
    ld      a, TILEMAP_DATA_SIZE & 0xFF
    out     (DMA_LEN_LO), a
    ld      a, TILEMAP_DATA_SIZE >> 8
    out     (DMA_LEN_HI), a

    ; Start DMA to VRAM
    ld      a, DMA_START
    out     (DMA_CTRL), a

    ret

; ==================================================================
; WAIT_VBLANK - Wait until VBlank starts
; ==================================================================
wait_vblank:
    ; First wait for any current VBlank to end
.wait_end:
    in      a, (VDP_STATUS)
    and     0x01
    jr      nz, .wait_end

    ; Now wait for next VBlank to start
.wait_start:
    in      a, (VDP_STATUS)
    and     0x01
    jr      z, .wait_start

    ret

; ==================================================================
; ROM DATA INCLUDES
; ==================================================================
    include "tiles.inc"
    include "palette.inc"
    include "tilemap.inc"

; ==================================================================
; ROM SIZE PADDING (ensure binary is reasonable size)
; ==================================================================
    ; Optional: Pad to specific size or add checksum
    ; For now, just end naturally

; ==================================================================
; END OF ROM
; ==================================================================
