; Super_Z80 Diagnostic Cartridge - Palette Data
; palette.inc - 16 palette entries, 9-bit RGB
;
; Palette format (per PPU spec):
;   uint16_t packed as:
;   Bits 0-2: Red   (0-7)
;   Bits 3-5: Green (0-7)
;   Bits 6-8: Blue  (0-7)
;   Bits 9-15: Reserved (0)
;
; Stored little-endian (low byte first)
; Formula: packed = R | (G << 3) | (B << 6)

; ==================================================================
; PALETTE DATA (16 entries = 32 bytes)
; ==================================================================
palette_data:
    ; Entry 0: Black (R=0, G=0, B=0)
    ; packed = 0 | (0<<3) | (0<<6) = 0x0000
    dw  0x0000

    ; Entry 1: Red (R=7, G=0, B=0) - will toggle with blue in ISR
    ; packed = 7 | (0<<3) | (0<<6) = 0x0007
    dw  0x0007

    ; Entry 2: Green (R=0, G=7, B=0)
    ; packed = 0 | (7<<3) | (0<<6) = 0x0038
    dw  0x0038

    ; Entry 3: Blue (R=0, G=0, B=7)
    ; packed = 0 | (0<<3) | (7<<6) = 0x01C0
    dw  0x01C0

    ; Entry 4: White (R=7, G=7, B=7)
    ; packed = 7 | (7<<3) | (7<<6) = 0x01FF
    dw  0x01FF

    ; Entry 5: Yellow (R=7, G=7, B=0)
    ; packed = 7 | (7<<3) | (0<<6) = 0x003F
    dw  0x003F

    ; Entry 6: Cyan (R=0, G=7, B=7)
    ; packed = 0 | (7<<3) | (7<<6) = 0x01F8
    dw  0x01F8

    ; Entry 7: Magenta (R=7, G=0, B=7)
    ; packed = 7 | (0<<3) | (7<<6) = 0x01C7
    dw  0x01C7

    ; Entries 8-15: Gradient (grayscale ramp)
    ; Entry 8: Dark gray (R=1, G=1, B=1) = 1|(1<<3)|(1<<6) = 0x0049
    dw  0x0049
    ; Entry 9: (R=2, G=2, B=2) = 2|(2<<3)|(2<<6) = 0x0092
    dw  0x0092
    ; Entry 10: (R=3, G=3, B=3) = 3|(3<<3)|(3<<6) = 0x00DB
    dw  0x00DB
    ; Entry 11: (R=4, G=4, B=4) = 4|(4<<3)|(4<<6) = 0x0124
    dw  0x0124
    ; Entry 12: (R=5, G=5, B=5) = 5|(5<<3)|(5<<6) = 0x016D
    dw  0x016D
    ; Entry 13: (R=6, G=6, B=6) = 6|(6<<3)|(6<<6) = 0x01B6
    dw  0x01B6
    ; Entry 14: Duplicate white
    dw  0x01FF
    ; Entry 15: Duplicate black
    dw  0x0000

PALETTE_DATA_SIZE   equ     32      ; 16 entries * 2 bytes

; Color constants for ISR palette toggle
COLOR_RED       equ     0x0007  ; R=7, G=0, B=0
COLOR_BLUE      equ     0x01C0  ; R=0, G=0, B=7
