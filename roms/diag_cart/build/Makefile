# Super_Z80 Diagnostic Cartridge - Build Makefile
#
# Assembler: sjasmplus (https://github.com/z00m128/sjasmplus)
# Output: out/diag_cart.bin

# Assembler selection (sjasmplus preferred)
# Use local sjasmplus if available, otherwise system one
ASM := ./sjasmplus

# Source and output directories
SRC_DIR := ../src
OUT_DIR := out

# Source files
MAIN_SRC := $(SRC_DIR)/diag_cart.asm
INC_FILES := $(wildcard $(SRC_DIR)/*.inc)

# Output file
OUTPUT := $(OUT_DIR)/diag_cart.bin

# Assembler flags for sjasmplus
# --raw: Output raw binary to file
# -I: Include path
# --lst: Generate listing file
ASM_FLAGS := -I$(SRC_DIR) --lst=$(OUT_DIR)/diag_cart.lst --raw=$(OUTPUT)

.PHONY: all clean

all: $(OUTPUT)

$(OUTPUT): $(MAIN_SRC) $(INC_FILES) | $(OUT_DIR)
	$(ASM) $(ASM_FLAGS) $(MAIN_SRC)
	@echo "Built: $(OUTPUT)"
	@ls -la $(OUTPUT)

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

clean:
	rm -rf $(OUT_DIR)

# Disassemble for debugging
disasm: $(OUTPUT)
	z80dasm -a -t -g 0x0000 $(OUTPUT) > $(OUT_DIR)/diag_cart.dis || true

# Show ROM info
info: $(OUTPUT)
	@echo "ROM Size: $$(stat -c%s $(OUTPUT)) bytes"
	@echo "Max Size: 32768 bytes (32KB)"
	@[ $$(stat -c%s $(OUTPUT)) -le 32768 ] && echo "OK: Within size limit" || echo "ERROR: ROM too large!"

.PHONY: disasm info
