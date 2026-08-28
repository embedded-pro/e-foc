Feature: FOC Loop Cycle Budget (QEMU SIL)
  The FOC hot-path shall execute within the real-time cycle budget.
  These scenarios validate the requirement on emulated ARM silicon using
  DWT CYCCNT under QEMU. Results are informational regression signals —
  the authoritative budget gate is the static cortex-cycle-budget analysis.

  Background:
    Given the QEMU SIL target is running

  @REQ-SIL-PERF-001
  Scenario: Torque cascade inner loop cycle count is recorded
    When the FOC hot paths are benchmarked on the emulated target
    Then the torque Calculate() cycle count is recorded

  @REQ-SIL-PERF-002
  Scenario: Speed cascade cycle count is recorded
    When the FOC hot paths are benchmarked on the emulated target
    Then the speed Calculate() cycle count is recorded

  @REQ-SIL-PERF-003
  Scenario: Position cascade cycle count is recorded
    When the FOC hot paths are benchmarked on the emulated target
    Then the position Calculate() cycle count is recorded
