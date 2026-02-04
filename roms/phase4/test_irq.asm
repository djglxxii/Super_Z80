; Phase 4 IRQ Test ROM
; This ROM validates the IRQ infrastructure (interrupt controller, I/O ports, /INT line)
; Requirements:
; - Configure IM 1 (interrupt mode 1, vector to 0x0038)
; - Enable TIMER interrupt source via IRQ_ENABLE port 0x81
; - ISR at 0x0038 must ACK interrupt and return
; - ISR increments counter in Work RAM to prove execution

    ORG 0x0000

; Entry point at reset
Reset:
    DI                      ; Disable interrupts during setup
    LD SP, 0xDFFF          ; Set stack pointer to end of Work RAM

    ; Configure interrupt mode 1 (vector to 0x0038)
    IM 1

    ; Enable TIMER interrupt source (bit 1)
    ; Port 0x81 = IRQ_ENABLE
    LD A, 0b00000010       ; Enable TIMER (bit 1)
    OUT (0x81), A

    ; Clear ISR entry counter in Work RAM
    LD HL, 0xC000          ; Counter address
    LD (HL), 0             ; Low byte
    INC HL
    LD (HL), 0             ; High byte

    ; Enable interrupts and enter main loop
    EI

MainLoop:
    ; Infinite loop - wait for interrupts
    NOP
    NOP
    NOP
    JP MainLoop

; Pad to ISR vector at 0x0038
    ORG 0x0038

; Interrupt Service Routine (ISR)
; This is called when /INT is asserted and CPU is in IM 1
ISR:
    ; Save registers
    PUSH AF
    PUSH HL

    ; Read IRQ_STATUS (port 0x80) - optional for debugging
    ; This read does NOT auto-clear pending bits
    IN A, (0x80)

    ; Increment ISR entry counter at 0xC000
    LD HL, 0xC000
    LD A, (HL)
    INC A
    LD (HL), A
    INC HL
    LD A, (HL)
    ADC A, 0               ; Add carry if low byte wrapped
    LD (HL), A

    ; ACK TIMER interrupt by writing to IRQ_ACK (port 0x82)
    ; Write-1-to-clear: bit 1 = TIMER
    LD A, 0b00000010
    OUT (0x82), A

    ; Restore registers
    POP HL
    POP AF

    ; Return from interrupt
    RETI

; Pad to fill ROM
    ORG 0x0100
EndOfROM:
