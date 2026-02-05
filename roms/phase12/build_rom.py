#!/usr/bin/env python3
"""
Phase 12 Audio Test ROM - Hand-assembled binary generator.
Produces test_audio.bin for the Super_Z80 emulator.

Test sequence:
1. PSG tone at ~440Hz, then pitch change to ~880Hz after ~2 seconds
2. YM2151 simple tone on channel 0
3. PCM trigger from ROM sample data
4. Loop with periodic PCM retrigger
"""

import struct
import math

rom = bytearray(0x8000)  # 32KB ROM, zero-filled

def emit(addr, data):
    """Write bytes at address."""
    if isinstance(data, int):
        rom[addr] = data & 0xFF
        return addr + 1
    for i, b in enumerate(data):
        rom[addr + i] = b & 0xFF
    return addr + len(data)

# I/O ports
IRQ_STATUS = 0x80
IRQ_ENABLE = 0x81
IRQ_ACK    = 0x82
PSG_DATA   = 0x60
OPM_ADDR   = 0x70
OPM_DATA   = 0x71
PCM0_START_LO = 0x72
PCM0_START_HI = 0x73
PCM0_LEN      = 0x74
PCM0_VOL      = 0x75
PCM0_CTRL     = 0x76
AUDIO_MASTER_VOL = 0x7C

# RAM addresses
FRAME_COUNT = 0xC000
FRAME_COUNT_HI = 0xC001
TEST_STATE  = 0xC002

# PCM sample data will be at this address in ROM
PCM_SAMPLE_ADDR = 0x0400  # 1024 in ROM
PCM_SAMPLE_LEN = 64

# =====================================================
# Z80 instruction encoding helpers
# =====================================================
def DI():      return [0xF3]
def EI():      return [0xFB]
def IM1():     return [0xED, 0x56]
def HALT():    return [0x76]
def RET():     return [0xC9]
def RETI():    return [0xED, 0x4D]
def NOP():     return [0x00]
def XOR_A():   return [0xAF]
def OR_A():    return [0xB7]

def LD_SP_nn(nn):  return [0x31, nn & 0xFF, (nn >> 8) & 0xFF]
def LD_A_n(n):     return [0x3E, n & 0xFF]
def LD_HL_nn(nn):  return [0x21, nn & 0xFF, (nn >> 8) & 0xFF]
def LD_DE_nn(nn):  return [0x11, nn & 0xFF, (nn >> 8) & 0xFF]
def LD_pnn_A(nn):  return [0x32, nn & 0xFF, (nn >> 8) & 0xFF]
def LD_pnn_HL(nn): return [0x22, nn & 0xFF, (nn >> 8) & 0xFF]
def LD_HL_pnn(nn): return [0x2A, nn & 0xFF, (nn >> 8) & 0xFF]
def LD_DE_pnn(nn): return [0xED, 0x5B, nn & 0xFF, (nn >> 8) & 0xFF]

def JP(nn):    return [0xC3, nn & 0xFF, (nn >> 8) & 0xFF]
def JR_NZ(offset): return [0x20, offset & 0xFF]
def JR_NC(offset): return [0x30, offset & 0xFF]
def JR(offset):    return [0x18, offset & 0xFF]
def CALL(nn):  return [0xCD, nn & 0xFF, (nn >> 8) & 0xFF]

def OUT_n_A(port): return [0xD3, port & 0xFF]
def IN_A_n(port):  return [0xDB, port & 0xFF]

def PUSH_AF(): return [0xF5]
def POP_AF():  return [0xF1]
def PUSH_HL(): return [0xE5]
def POP_HL():  return [0xE1]

def INC_HL():  return [0x23]
def ADD_HL_DE(): return [0x19]
def SBC_HL_DE(): return [0xED, 0x52]

# =====================================================
# Entry point at 0x0000
# =====================================================
pc = 0x0000
pc = emit(pc, DI())
pc = emit(pc, LD_SP_nn(0xFFFF))
pc = emit(pc, JP(0x0080))  # Jump to init

# =====================================================
# ISR at 0x0038 (IM 1 handler)
# =====================================================
pc = 0x0038
pc = emit(pc, PUSH_AF())
pc = emit(pc, PUSH_HL())

# Acknowledge IRQ
pc = emit(pc, IN_A_n(IRQ_STATUS))
pc = emit(pc, OUT_n_A(IRQ_ACK))

# Increment frame counter
pc = emit(pc, LD_HL_pnn(FRAME_COUNT))
pc = emit(pc, INC_HL())
pc = emit(pc, LD_pnn_HL(FRAME_COUNT))

pc = emit(pc, POP_HL())
pc = emit(pc, POP_AF())
pc = emit(pc, EI())
pc = emit(pc, RETI())

# =====================================================
# Wait 120 frames subroutine (at 0x0060)
# =====================================================
WAIT_SUB = 0x0060
pc = WAIT_SUB

# LD HL, (FRAME_COUNT)
pc = emit(pc, LD_HL_pnn(FRAME_COUNT))
# LD DE, 120
pc = emit(pc, LD_DE_nn(120))
# ADD HL, DE
pc = emit(pc, ADD_HL_DE())
# HL = target count

# .wait_loop:
wait_loop_addr = pc
pc = emit(pc, HALT())
pc = emit(pc, PUSH_HL())
# LD DE, (FRAME_COUNT)
pc = emit(pc, LD_DE_pnn(FRAME_COUNT))
# OR A (clear carry)
pc = emit(pc, OR_A())
# SBC HL, DE
pc = emit(pc, SBC_HL_DE())
pc = emit(pc, POP_HL())
# JR NC, wait_loop (if HL > FRAME_COUNT, keep waiting)
jr_offset = wait_loop_addr - (pc + 2)  # relative offset from after JR instruction
pc = emit(pc, JR_NC(jr_offset & 0xFF))
pc = emit(pc, RET())

# =====================================================
# Init at 0x0080
# =====================================================
pc = 0x0080

# Clear RAM variables
pc = emit(pc, XOR_A())
pc = emit(pc, LD_pnn_A(FRAME_COUNT))
pc = emit(pc, LD_pnn_A(FRAME_COUNT_HI))
pc = emit(pc, LD_pnn_A(TEST_STATE))

# Set master volume to max
pc = emit(pc, LD_A_n(0xFF))
pc = emit(pc, OUT_n_A(AUDIO_MASTER_VOL))

# Enable VBlank IRQ
pc = emit(pc, LD_A_n(0x01))
pc = emit(pc, OUT_n_A(IRQ_ENABLE))
pc = emit(pc, IM1())
pc = emit(pc, EI())

# =====================================================
# Step 1: PSG tone at ~440Hz
# Period = 3579545 / (2 * 440 * 16) = 254 = 0x0FE
# =====================================================

# Channel 0 volume = max (0)
pc = emit(pc, LD_A_n(0x90))   # Latch ch0 vol, data=0
pc = emit(pc, OUT_n_A(PSG_DATA))

# Channel 0 tone period = 0x0FE
# Latch byte: 1 00 0 1110 = 0x8E
pc = emit(pc, LD_A_n(0x8E))   # Latch ch0 tone, low 4 bits = 0xE
pc = emit(pc, OUT_n_A(PSG_DATA))
# Data byte: 0 0 001111 = 0x0F
pc = emit(pc, LD_A_n(0x0F))   # High 6 bits = 0x0F
pc = emit(pc, OUT_n_A(PSG_DATA))

# Silence other channels
pc = emit(pc, LD_A_n(0xBF))   # Ch1 vol off
pc = emit(pc, OUT_n_A(PSG_DATA))
pc = emit(pc, LD_A_n(0xDF))   # Ch2 vol off
pc = emit(pc, OUT_n_A(PSG_DATA))
pc = emit(pc, LD_A_n(0xFF))   # Noise vol off
pc = emit(pc, OUT_n_A(PSG_DATA))

# Wait ~2 seconds
pc = emit(pc, CALL(WAIT_SUB))

# Change to ~880Hz: period = 127 = 0x07F
# Latch: 1 00 0 1111 = 0x8F
pc = emit(pc, LD_A_n(0x8F))   # Latch ch0 tone, low 4 bits = 0xF
pc = emit(pc, OUT_n_A(PSG_DATA))
# Data: 0 0 000111 = 0x07
pc = emit(pc, LD_A_n(0x07))   # High 6 bits = 0x07
pc = emit(pc, OUT_n_A(PSG_DATA))

# =====================================================
# Step 2: YM2151 - simple tone on channel 0
# Algorithm 7, only M1 active
# =====================================================

# Helper: write OPM register
def opm_write(reg, val):
    return LD_A_n(reg) + OUT_n_A(OPM_ADDR) + LD_A_n(val) + OUT_n_A(OPM_DATA)

# RL/FB/CON: RL=3(both), FB=0, CON=7
pc = emit(pc, opm_write(0x20, 0xC7))
# DT1/MUL for M1 (reg 0x40): DT1=0, MUL=1
pc = emit(pc, opm_write(0x40, 0x01))
# TL for M1 (reg 0x60): moderate volume
pc = emit(pc, opm_write(0x60, 0x10))
# KS/AR for M1 (reg 0x80): AR=31 (fast attack)
pc = emit(pc, opm_write(0x80, 0x1F))
# D1R for M1 (reg 0xA0): 0
pc = emit(pc, opm_write(0xA0, 0x00))
# D2R for M1 (reg 0xC0): 0
pc = emit(pc, opm_write(0xC0, 0x00))
# D1L/RR for M1 (reg 0xE0): D1L=0, RR=15
pc = emit(pc, opm_write(0xE0, 0x0F))

# Silence other operators (TL = 0x7F)
pc = emit(pc, opm_write(0x68, 0x7F))  # M2
pc = emit(pc, opm_write(0x70, 0x7F))  # C1
pc = emit(pc, opm_write(0x78, 0x7F))  # C2

# KC for ch0 (reg 0x28): Octave 4, note A
pc = emit(pc, opm_write(0x28, 0x4A))

# Key On: reg 0x08, ch0 all ops on
pc = emit(pc, opm_write(0x08, 0x78))

# =====================================================
# Step 3: PCM - trigger a sample on channel 0
# =====================================================

# PCM start address
pcm_lo = PCM_SAMPLE_ADDR & 0xFF
pcm_hi = (PCM_SAMPLE_ADDR >> 8) & 0xFF

pc = emit(pc, LD_A_n(pcm_lo))
pc = emit(pc, OUT_n_A(PCM0_START_LO))
pc = emit(pc, LD_A_n(pcm_hi))
pc = emit(pc, OUT_n_A(PCM0_START_HI))

# Length
pc = emit(pc, LD_A_n(PCM_SAMPLE_LEN))
pc = emit(pc, OUT_n_A(PCM0_LEN))

# Volume (~75%)
pc = emit(pc, LD_A_n(0xC0))
pc = emit(pc, OUT_n_A(PCM0_VOL))

# Trigger
pc = emit(pc, LD_A_n(0x01))
pc = emit(pc, OUT_n_A(PCM0_CTRL))
# Clear trigger
pc = emit(pc, LD_A_n(0x00))
pc = emit(pc, OUT_n_A(PCM0_CTRL))

# =====================================================
# Main loop: wait and retrigger PCM
# =====================================================
main_loop_addr = pc

pc = emit(pc, CALL(WAIT_SUB))

# Retrigger PCM0
pc = emit(pc, LD_A_n(0x01))
pc = emit(pc, OUT_n_A(PCM0_CTRL))
pc = emit(pc, LD_A_n(0x00))
pc = emit(pc, OUT_n_A(PCM0_CTRL))

# Check BUSY
pc = emit(pc, IN_A_n(PCM0_CTRL))

# Jump back to main loop
pc = emit(pc, JP(main_loop_addr))

# =====================================================
# PCM Sample Data at PCM_SAMPLE_ADDR
# Short decaying click waveform, 8-bit unsigned
# =====================================================
sample_data = [
    0x80, 0xB0, 0xD0, 0xE8, 0xF8, 0xFF, 0xF8, 0xE8,
    0xD0, 0xB0, 0x80, 0x50, 0x30, 0x18, 0x08, 0x00,
    0x08, 0x18, 0x30, 0x50, 0x80, 0xA8, 0xC8, 0xE0,
    0xF0, 0xF8, 0xF0, 0xE0, 0xC8, 0xA8, 0x80, 0x58,
    0x38, 0x20, 0x10, 0x08, 0x10, 0x20, 0x38, 0x58,
    0x80, 0xA0, 0xB8, 0xC8, 0xD0, 0xD4, 0xD0, 0xC8,
    0xB8, 0xA0, 0x80, 0x60, 0x48, 0x38, 0x30, 0x2C,
    0x30, 0x38, 0x48, 0x60, 0x80, 0x90, 0x98, 0x98,
]
assert len(sample_data) == PCM_SAMPLE_LEN
emit(PCM_SAMPLE_ADDR, sample_data)

# =====================================================
# Write ROM file
# =====================================================
# Trim to actual used size (don't write full 32KB of zeros)
# Find last non-zero byte
last_used = 0
for i in range(len(rom) - 1, -1, -1):
    if rom[i] != 0:
        last_used = i
        break

# Round up to next 256 byte boundary
rom_size = ((last_used + 256) // 256) * 256
output = rom[:rom_size]

with open("test_audio.bin", "wb") as f:
    f.write(output)

print(f"Phase 12 Audio Test ROM generated: test_audio.bin ({len(output)} bytes)")
print(f"  PCM sample data at 0x{PCM_SAMPLE_ADDR:04X} ({PCM_SAMPLE_LEN} bytes)")
print(f"  Code ends at 0x{pc:04X}")
print(f"  ISR at 0x0038")
print(f"  Wait subroutine at 0x{WAIT_SUB:04X}")
print(f"  Main loop at 0x{main_loop_addr:04X}")
