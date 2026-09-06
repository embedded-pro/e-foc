Feature: FOC Loop Cycle Budget (QEMU SIL)
  The FOC hot-path shall execute within the real-time cycle budget.
  The steps below are not implemented yet: they are tagged @wip and claim no
  requirement coverage. The authoritative budget gate is the static
  cortex-cycle-budget analysis.

  Background:
    Given the QEMU SIL target is running

  @wip
  Scenario: Torque cascade inner loop cycle count is recorded
    When the FOC hot paths are benchmarked on the emulated target
    Then the torque Calculate() cycle count is recorded

  @wip
  Scenario: Speed cascade cycle count is recorded
    When the FOC hot paths are benchmarked on the emulated target
    Then the speed Calculate() cycle count is recorded

  @wip
  Scenario: Position cascade cycle count is recorded
    When the FOC hot paths are benchmarked on the emulated target
    Then the position Calculate() cycle count is recorded
