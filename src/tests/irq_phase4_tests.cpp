// Phase 4 IRQ Infrastructure Tests
// Validates IRQController, I/O ports, synthetic trigger, and /INT line behavior

#include <cassert>
#include <cstdio>
#include "console/SuperZ80Console.h"
#include "devices/irq/IRQController.h"
#include "core/log/Logger.h"

using namespace sz;

void TestIRQControllerBasics() {
    printf("=== Test 1: IRQController Basic Operations ===\n");

    irq::IRQController irq;
    irq.Reset();

    // Initially, no pending, no enable, /INT deasserted
    assert(irq.ReadStatus() == 0);
    assert(irq.ReadEnable() == 0);
    assert(irq.IntLineAsserted() == false);
    printf("✓ Initial state correct\n");

    // Raise TIMER pending (bit 1)
    irq.Raise(static_cast<u8>(irq::IrqBit::Timer));
    assert(irq.ReadStatus() == 0x02);
    assert(irq.IntLineAsserted() == false);  // Not enabled yet
    printf("✓ Pending latched, /INT remains low (masked)\n");

    // Enable TIMER
    irq.WriteEnable(0x02);
    irq.PreCpuUpdate();
    assert(irq.ReadEnable() == 0x02);
    assert(irq.IntLineAsserted() == true);   // Now asserted
    printf("✓ /INT asserted when pending & enabled\n");

    // ACK TIMER (write-1-to-clear)
    irq.Ack(0x02);
    assert(irq.ReadStatus() == 0x00);
    assert(irq.IntLineAsserted() == false);  // Should drop immediately
    printf("✓ ACK clears pending, /INT drops immediately\n");

    printf("Test 1 PASSED\n\n");
}

void TestIRQMaskingBehavior() {
    printf("=== Test 2: IRQ Masking Behavior ===\n");

    irq::IRQController irq;
    irq.Reset();

    // Raise multiple sources
    irq.Raise(0x07);  // VBlank | Timer | Scanline
    assert(irq.ReadStatus() == 0x07);
    assert(irq.IntLineAsserted() == false);  // No enable
    printf("✓ Multiple pending, /INT low (all masked)\n");

    // Enable only Timer
    irq.WriteEnable(0x02);
    irq.PreCpuUpdate();
    assert(irq.IntLineAsserted() == true);
    printf("✓ /INT high (Timer enabled and pending)\n");

    // ACK Timer only
    irq.Ack(0x02);
    assert(irq.ReadStatus() == 0x05);  // VBlank | Scanline remain
    assert(irq.IntLineAsserted() == false);  // Timer cleared, others masked
    printf("✓ ACK Timer, /INT drops (others still pending but masked)\n");

    // Enable Scanline
    irq.WriteEnable(0x06);  // Timer | Scanline
    irq.PreCpuUpdate();
    assert(irq.IntLineAsserted() == true);  // Scanline is pending & enabled
    printf("✓ Enable Scanline, /INT asserts (Scanline pending)\n");

    printf("Test 2 PASSED\n\n");
}

void TestSyntheticIRQTrigger() {
    printf("=== Test 3: Synthetic IRQ Trigger ===\n");

    console::SuperZ80Console console;
    console.PowerOn();
    console.Reset();

    auto irq_state = console.GetIRQDebugState();
    assert(irq_state.synthetic_fire_count == 0);
    assert(irq_state.pending == 0);
    printf("✓ Initial state: no synthetic fires, no pending\n");

    // Run to scanline 10 (synthetic trigger fires here)
    for (int i = 0; i < 10; ++i) {
        auto sched_state = console.GetSchedulerDebugState();
        if (sched_state.current_scanline == 10) {
            break;
        }
        console.StepFrame();
    }

    // Step one more frame to ensure scanline 10 is reached
    console.StepFrame();

    irq_state = console.GetIRQDebugState();
    assert(irq_state.synthetic_fire_count >= 1);
    // Note: pending will be 0x02 if enabled, or latched regardless
    printf("✓ Synthetic TIMER IRQ fired at scanline 10, count=%llu\n",
           static_cast<unsigned long long>(irq_state.synthetic_fire_count));

    // Verify once-per-frame trigger
    u64 fire_count_before = irq_state.synthetic_fire_count;
    console.StepFrame();
    irq_state = console.GetIRQDebugState();
    assert(irq_state.synthetic_fire_count == fire_count_before + 1);
    printf("✓ Synthetic trigger fires once per frame\n");

    printf("Test 3 PASSED\n\n");
}

void TestIOPortSemantics() {
    printf("=== Test 4: I/O Port Semantics ===\n");

    // This test validates Bus I/O decoding
    // We'll create a minimal test by using IRQController directly
    // (Full bus test would require CPU execution)

    irq::IRQController irq;
    irq.Reset();

    // Simulate: OUT (0x81), 0x02  (enable TIMER)
    irq.WriteEnable(0x02);
    assert(irq.ReadEnable() == 0x02);
    printf("✓ Port 0x81 (IRQ_ENABLE) write/read\n");

    // Simulate: Raise TIMER
    irq.Raise(0x02);
    assert(irq.ReadStatus() == 0x02);
    printf("✓ Port 0x80 (IRQ_STATUS) read shows pending\n");

    // Verify read does not auto-clear
    assert(irq.ReadStatus() == 0x02);
    assert(irq.ReadStatus() == 0x02);
    printf("✓ Port 0x80 read does NOT auto-clear\n");

    // Simulate: OUT (0x82), 0x02  (ACK TIMER)
    irq.Ack(0x02);
    assert(irq.ReadStatus() == 0x00);
    printf("✓ Port 0x82 (IRQ_ACK) write-1-to-clear\n");

    printf("Test 4 PASSED\n\n");
}

void TestImmediateDrop() {
    printf("=== Test 5: Immediate /INT Drop ===\n");

    irq::IRQController irq;
    irq.Reset();

    // Raise and enable TIMER
    irq.Raise(0x02);
    irq.WriteEnable(0x02);
    irq.PreCpuUpdate();
    assert(irq.IntLineAsserted() == true);
    printf("✓ /INT asserted\n");

    // ACK TIMER - /INT must drop in same scanline step
    irq.Ack(0x02);
    // No need to call PostCpuUpdate here - Ack does immediate recompute
    assert(irq.IntLineAsserted() == false);
    printf("✓ /INT drops immediately after ACK (same scanline step)\n");

    // Test with WriteEnable
    irq.Raise(0x02);
    irq.WriteEnable(0x02);
    assert(irq.IntLineAsserted() == true);
    irq.WriteEnable(0x00);  // Disable all
    assert(irq.IntLineAsserted() == false);
    printf("✓ /INT drops immediately when enable mask clears\n");

    printf("Test 5 PASSED\n\n");
}

int main() {
    SZ_LOG_INFO("=== Phase 4 IRQ Infrastructure Tests ===");

    TestIRQControllerBasics();
    TestIRQMaskingBehavior();
    TestSyntheticIRQTrigger();
    TestIOPortSemantics();
    TestImmediateDrop();

    printf("========================================\n");
    printf("ALL PHASE 4 TESTS PASSED ✓\n");
    printf("========================================\n");

    return 0;
}
