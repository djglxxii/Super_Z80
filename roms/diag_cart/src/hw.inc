; Super_Z80 Diagnostic Cartridge - Hardware Constants
; hw.inc - Port constants, RAM addresses, page sizes

; ==================================================================
; VIDEO CONTROL PORTS (0x10-0x1F)
; ==================================================================
VDP_STATUS      equ     0x10    ; VBlank flag (bit 0 = 1 during scanlines 192-261)
VDP_CTRL        equ     0x11    ; Display control register
SCROLL_X        equ     0x12    ; Plane A horizontal scroll
SCROLL_Y        equ     0x13    ; Plane A vertical scroll
PLANE_A_BASE    equ     0x16    ; Plane A tilemap base (page index)
PATTERN_BASE    equ     0x18    ; Tile pattern base (page index)
PAL_ADDR        equ     0x1E    ; Palette address (byte 0-255)
PAL_DATA        equ     0x1F    ; Palette data (auto-increment)

; VDP_CTRL bit definitions
VDP_DISPLAY_EN  equ     0x01    ; Bit 0: Display enable
VDP_PLANE_A_EN  equ     0x02    ; Bit 1: Plane A enable
VDP_PLANE_B_EN  equ     0x04    ; Bit 2: Plane B enable (must be off)
VDP_SPRITES_EN  equ     0x08    ; Bit 3: Sprites enable (must be off)

; ==================================================================
; DMA PORTS (0x30-0x36)
; ==================================================================
DMA_SRC_LO      equ     0x30    ; Source address low byte
DMA_SRC_HI      equ     0x31    ; Source address high byte
DMA_DST_LO      equ     0x32    ; Destination address low byte
DMA_DST_HI      equ     0x33    ; Destination address high byte
DMA_LEN_LO      equ     0x34    ; Transfer length low byte
DMA_LEN_HI      equ     0x35    ; Transfer length high byte
DMA_CTRL        equ     0x36    ; DMA control register

; DMA_CTRL bit definitions
DMA_START       equ     0x01    ; Bit 0: Start DMA
DMA_QUEUE       equ     0x04    ; Bit 2: Queue if not in VBlank
DMA_PAL_DST     equ     0x08    ; Bit 3: Destination is Palette RAM

; ==================================================================
; IRQ PORTS (0x80-0x82)
; ==================================================================
IRQ_STATUS      equ     0x80    ; Read: pending IRQ bits
IRQ_ENABLE      equ     0x81    ; Read/Write: IRQ enable mask
IRQ_ACK         equ     0x82    ; Write: acknowledge IRQs (write-1-to-clear)

; IRQ bit definitions
IRQ_VBLANK      equ     0x01    ; Bit 0: VBlank IRQ

; ==================================================================
; MEMORY MAP
; ==================================================================
; ROM:  0x0000 - 0x7FFF (32KB max, bank 0 only)
; VRAM: (internal to PPU, accessed via DMA)
; WRAM: 0xC000 - 0xFFFF (16KB)

; ==================================================================
; RAM VARIABLES (fixed addresses per spec)
; ==================================================================
FRAME_COUNTER   equ     0xC000  ; 16-bit frame counter
VBLANK_COUNT    equ     0xC002  ; 16-bit VBlank ISR counter
ISR_ENTRY_COUNT equ     0xC004  ; 16-bit ISR entry counter
SCRATCH         equ     0xC006  ; Scratch byte

; DMA source buffers
DMA_PAL_BUF     equ     0xC100  ; Palette DMA buffer (32 bytes: 0xC100-0xC11F)
DMA_TILE_BUF    equ     0xC120  ; Tile pattern buffer (96 bytes: 0xC120-0xC17F)
TILEMAP_BUF     equ     0xC200  ; Tilemap buffer (varies)

; ==================================================================
; PAGE SIZES AND CONSTANTS
; ==================================================================
PAGE_SIZE       equ     2048    ; 2KB per page for base selectors
TILE_SIZE       equ     24      ; 8x8 tile at 4bpp = 24 bytes (3 bytes/row * 8 rows)
                                ; Wait - 4bpp means 4 bits per pixel
                                ; 8 pixels per row = 32 bits = 4 bytes per row
                                ; 8 rows = 32 bytes per tile
TILE_SIZE_4BPP  equ     32      ; Corrected: 8x8 @ 4bpp = 32 bytes

; Palette
PAL_ENTRIES     equ     16      ; Using 16 palette entries for diagnostics
PAL_BYTE_SIZE   equ     32      ; 16 entries * 2 bytes each = 32 bytes

; Tilemap
TILEMAP_WIDTH   equ     32      ; Tiles per row
TILEMAP_HEIGHT  equ     24      ; Tile rows
TILEMAP_ENTRY   equ     2       ; 2 bytes per tilemap entry (tile index + attributes)
TILEMAP_SIZE    equ     TILEMAP_WIDTH * TILEMAP_HEIGHT * TILEMAP_ENTRY  ; 1536 bytes

; Screen
SCREEN_WIDTH    equ     256
SCREEN_HEIGHT   equ     192
VBLANK_START    equ     192     ; VBlank begins at scanline 192
VBLANK_END      equ     261     ; VBlank ends at scanline 261

; ==================================================================
; DMA DESTINATION CONVENTIONS
; ==================================================================
; When DST_IS_PALETTE bit is set in DMA_CTRL:
;   DMA_DST is a byte offset into palette RAM (0-255)
; Otherwise:
;   DMA_DST is a VRAM address

; Pattern base: page 0 = VRAM offset 0x0000
; Plane A base: page 0 = VRAM offset 0x0000 (for tilemap)
; We'll use page 0 for patterns, page 1 for tilemap
; Actually, per spec: pick deterministic base page values (e.g., 0 for both)
PATTERN_PAGE    equ     0       ; Pattern data at page 0 (VRAM 0x0000)
TILEMAP_PAGE    equ     1       ; Tilemap at page 1 (VRAM 0x0800)

PATTERN_VRAM    equ     PATTERN_PAGE * PAGE_SIZE    ; 0x0000
TILEMAP_VRAM    equ     TILEMAP_PAGE * PAGE_SIZE    ; 0x0800
