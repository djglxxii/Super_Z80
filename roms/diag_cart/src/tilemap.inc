; Super_Z80 Diagnostic Cartridge - Tilemap Data
; tilemap.inc - 32x24 tilemap generator macros
;
; Tilemap entry format (2 bytes per tile):
;   Byte 0: Tile index (0-255)
;   Byte 1: Attributes
;           Bits 0-3: Palette selector (0-15)
;           Bit 4:    Horizontal flip
;           Bit 5:    Vertical flip
;           Bits 6-7: Priority (0 = normal)
;
; Pattern: Each row repeats [0 1 2 3 0 1 2 3 ...]
; Attributes: Palette 0, no flip, normal priority = 0x00

; ==================================================================
; TILEMAP ROW MACRO
; ==================================================================
; Generates one row of 32 tiles with pattern [0 1 2 3] repeated 8 times
; Each tile entry is 2 bytes: tile_index, attributes
TILEMAP_ROW     MACRO
    db  0, 0    ; Tile 0, attrs 0
    db  1, 0    ; Tile 1, attrs 0
    db  2, 0    ; Tile 2, attrs 0
    db  3, 0    ; Tile 3, attrs 0
    db  0, 0
    db  1, 0
    db  2, 0
    db  3, 0
    db  0, 0
    db  1, 0
    db  2, 0
    db  3, 0
    db  0, 0
    db  1, 0
    db  2, 0
    db  3, 0
    db  0, 0
    db  1, 0
    db  2, 0
    db  3, 0
    db  0, 0
    db  1, 0
    db  2, 0
    db  3, 0
    db  0, 0
    db  1, 0
    db  2, 0
    db  3, 0
    db  0, 0
    db  1, 0
    db  2, 0
    db  3, 0
ENDM

; ==================================================================
; FULL TILEMAP (32x24 = 768 tiles = 1536 bytes)
; ==================================================================
tilemap_data:
    TILEMAP_ROW     ; Row 0
    TILEMAP_ROW     ; Row 1
    TILEMAP_ROW     ; Row 2
    TILEMAP_ROW     ; Row 3
    TILEMAP_ROW     ; Row 4
    TILEMAP_ROW     ; Row 5
    TILEMAP_ROW     ; Row 6
    TILEMAP_ROW     ; Row 7
    TILEMAP_ROW     ; Row 8
    TILEMAP_ROW     ; Row 9
    TILEMAP_ROW     ; Row 10
    TILEMAP_ROW     ; Row 11
    TILEMAP_ROW     ; Row 12
    TILEMAP_ROW     ; Row 13
    TILEMAP_ROW     ; Row 14
    TILEMAP_ROW     ; Row 15
    TILEMAP_ROW     ; Row 16
    TILEMAP_ROW     ; Row 17
    TILEMAP_ROW     ; Row 18
    TILEMAP_ROW     ; Row 19
    TILEMAP_ROW     ; Row 20
    TILEMAP_ROW     ; Row 21
    TILEMAP_ROW     ; Row 22
    TILEMAP_ROW     ; Row 23

TILEMAP_DATA_SIZE   equ     1536    ; 32 * 24 * 2 bytes
