; Super_Z80 Diagnostic Cartridge - Tile Patterns
; tiles.inc - 4 tiles, 8x8, 4bpp (32 bytes each)
;
; 4bpp format: 4 bits per pixel, 8 pixels per row
; Each row = 4 bytes (8 pixels * 4 bits / 8 bits per byte)
; Each tile = 8 rows * 4 bytes = 32 bytes
;
; Pixel order in each byte: high nibble = left pixel, low nibble = right pixel
; So for a row of 8 pixels [P0 P1 P2 P3 P4 P5 P6 P7]:
;   Byte 0 = (P0 << 4) | P1
;   Byte 1 = (P2 << 4) | P3
;   Byte 2 = (P4 << 4) | P5
;   Byte 3 = (P6 << 4) | P7

; ==================================================================
; TILE 0: Solid color (palette entry 1 = red/blue, toggles in ISR)
; ==================================================================
; All pixels use palette index 1
tile_0_data:
    db  0x11, 0x11, 0x11, 0x11  ; Row 0: 1 1 1 1 1 1 1 1
    db  0x11, 0x11, 0x11, 0x11  ; Row 1
    db  0x11, 0x11, 0x11, 0x11  ; Row 2
    db  0x11, 0x11, 0x11, 0x11  ; Row 3
    db  0x11, 0x11, 0x11, 0x11  ; Row 4
    db  0x11, 0x11, 0x11, 0x11  ; Row 5
    db  0x11, 0x11, 0x11, 0x11  ; Row 6
    db  0x11, 0x11, 0x11, 0x11  ; Row 7

; ==================================================================
; TILE 1: Checkerboard (palette entries 2 and 3 = green and blue)
; ==================================================================
; Alternating 2x2 blocks of palette 2 and 3
tile_1_data:
    db  0x22, 0x33, 0x22, 0x33  ; Row 0: 2 2 3 3 2 2 3 3
    db  0x22, 0x33, 0x22, 0x33  ; Row 1: 2 2 3 3 2 2 3 3
    db  0x33, 0x22, 0x33, 0x22  ; Row 2: 3 3 2 2 3 3 2 2
    db  0x33, 0x22, 0x33, 0x22  ; Row 3: 3 3 2 2 3 3 2 2
    db  0x22, 0x33, 0x22, 0x33  ; Row 4
    db  0x22, 0x33, 0x22, 0x33  ; Row 5
    db  0x33, 0x22, 0x33, 0x22  ; Row 6
    db  0x33, 0x22, 0x33, 0x22  ; Row 7

; ==================================================================
; TILE 2: Vertical stripes (palette entries 4 and 0 = white and black)
; ==================================================================
; Alternating columns of palette 4 (white) and 0 (black)
tile_2_data:
    db  0x40, 0x40, 0x40, 0x40  ; Row 0: 4 0 4 0 4 0 4 0
    db  0x40, 0x40, 0x40, 0x40  ; Row 1
    db  0x40, 0x40, 0x40, 0x40  ; Row 2
    db  0x40, 0x40, 0x40, 0x40  ; Row 3
    db  0x40, 0x40, 0x40, 0x40  ; Row 4
    db  0x40, 0x40, 0x40, 0x40  ; Row 5
    db  0x40, 0x40, 0x40, 0x40  ; Row 6
    db  0x40, 0x40, 0x40, 0x40  ; Row 7

; ==================================================================
; TILE 3: Horizontal stripes (palette entries 4 and 0 = white and black)
; ==================================================================
; Alternating rows of palette 4 (white) and 0 (black)
tile_3_data:
    db  0x44, 0x44, 0x44, 0x44  ; Row 0: 4 4 4 4 4 4 4 4
    db  0x00, 0x00, 0x00, 0x00  ; Row 1: 0 0 0 0 0 0 0 0
    db  0x44, 0x44, 0x44, 0x44  ; Row 2
    db  0x00, 0x00, 0x00, 0x00  ; Row 3
    db  0x44, 0x44, 0x44, 0x44  ; Row 4
    db  0x00, 0x00, 0x00, 0x00  ; Row 5
    db  0x44, 0x44, 0x44, 0x44  ; Row 6
    db  0x00, 0x00, 0x00, 0x00  ; Row 7

; Total tile data size: 4 tiles * 32 bytes = 128 bytes
TILE_DATA_SIZE  equ     128
