# Phase 12 Audio Test ROM Makefile
# Requires z80asm (or compatible assembler)

ASM = z80asm
ASMFLAGS = -b

ROMS = test_audio.bin

all: $(ROMS)

%.bin: %.asm
	$(ASM) $(ASMFLAGS) -o $@ $<

clean:
	rm -f $(ROMS)

.PHONY: all clean
