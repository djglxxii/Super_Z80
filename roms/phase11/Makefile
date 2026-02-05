# Phase 11 Sprite Test ROM Makefile
# Requires z80asm (or compatible assembler)

ASM = z80asm
ASMFLAGS = -b

ROMS = test_sprites.bin

all: $(ROMS)

%.bin: %.asm
	$(ASM) $(ASMFLAGS) -o $@ $<

clean:
	rm -f $(ROMS)

.PHONY: all clean
