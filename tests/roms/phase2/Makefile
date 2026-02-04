# Phase 2 Test ROM Makefile
# Requires z80asm (or compatible assembler)

ASM = z80asm
ASMFLAGS = -b

ROMS = ram_rw_test.bin \
       open_bus_test.bin \
       unmapped_write_test.bin \
       io_stub_test.bin \
       reset_bank0_test.bin

all: $(ROMS)

%.bin: %.asm
	$(ASM) $(ASMFLAGS) -o $@ $<

clean:
	rm -f $(ROMS)

.PHONY: all clean
