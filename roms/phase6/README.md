# Phase 6: DMA Test ROM

## Purpose
Validates DMA (Direct Memory Access) functionality during VBlank.

## Test Cases

### Test 1: Positive DMA during VBlank
- Waits for VBlank (VDP_STATUS bit 0 = 1)
- Programs DMA to copy 256 bytes from RAM[0xC100] to VRAM[0x2000]
- DMA executes immediately (instantaneous)
- **Expected**: VRAM[0x2000..0x20FF] contains pattern 0x00..0xFF

### Test 2: Negative - Mid-frame DMA with QUEUE=0
- Waits for visible scanlines (VDP_STATUS bit 0 = 0)
- Programs DMA to copy 128 bytes to VRAM[0x3000]
- DMA_CTRL has START bit but NOT QUEUE bit
- **Expected**: DMA is ignored, VRAM[0x3000..0x307F] remains 0x00

### Test 3: Queue - Mid-frame DMA with QUEUE=1
- Waits for visible scanlines (VDP_STATUS bit 0 = 0)
- Programs DMA to copy 256 bytes to VRAM[0x4000]
- DMA_CTRL has both START and QUEUE_IF_NOT_VBLANK bits
- **Expected**: DMA queues and executes at next VBlank (scanline 192)
- **Expected**: VRAM[0x4000..0x40FF] contains pattern 0x00..0xFF

## DMA I/O Ports

| Port | Name | Access | Description |
|------|------|--------|-------------|
| 0x30 | DMA_SRC_LO | R/W | Source address low byte (CPU space) |
| 0x31 | DMA_SRC_HI | R/W | Source address high byte |
| 0x32 | DMA_DST_LO | R/W | Destination address low byte (VRAM) |
| 0x33 | DMA_DST_HI | R/W | Destination address high byte |
| 0x34 | DMA_LEN_LO | R/W | Length low byte |
| 0x35 | DMA_LEN_HI | R/W | Length high byte |
| 0x36 | DMA_CTRL | R/W | Control register |

### DMA_CTRL Bits
- Bit 0: START (write-1 to trigger DMA)
- Bit 1: QUEUE_IF_NOT_VBLANK (queue if not in VBlank)
- Bit 7: BUSY (read-only, always 0 since DMA is instantaneous)

## Verification

Emulator-side checks should verify:
1. VRAM[0x2000..0x20FF] = 0x00..0xFF after test 1
2. VRAM[0x3000..0x307F] = 0x00 (unchanged) after test 2
3. VRAM[0x4000..0x40FF] = 0x00..0xFF after test 3 completes at next VBlank

## Build

To assemble:
```bash
z80asm -o test_dma.bin test_dma.asm
```

Or use pasmo/sjasm/other Z80 assembler of choice.
