Feature: Control Loop Timing
  The control interrupt measures how long each execution of its callback takes
  and counts the executions that overrun the budget, miss the control period or
  re-enter. The mode scenarios boot calibrated and clear those counters before
  the rotor is aligned on command, so the window covers alignment as well as
  closed-loop running. The calibration scenario clears them before a full
  calibration is run from the terminal, so the window covers every
  identification procedure and the alignment between them. Each scenario then
  asks the target for the counters.

  The emulated core has no DWT cycle counter, so the emulated platform times the
  callback with a free-running timer on the 25 MHz system clock instead. The
  emulator's clock is instruction-counted at eight nanoseconds an instruction,
  so one cycle of that clock is five instructions retired and a run reproduces
  its own durations to within a cycle. The budget is three quarters of the
  control period, 937 cycles at 20 kHz. It bounds instructions retired, not the
  cycles of a real core, and complements the static cortex-cycle-budget gate
  rather than replacing it.

  The budget alone would let the callback grow some eightfold before failing,
  so each row also pins its slowest execution, at one and a half times the
  worst value repeated runs measured for the row, rounded up to ten. Those runs
  measured 103 to 104 cycles for every current law in torque mode, 119 in speed
  and position mode and 236 through a full calibration, the same to within a
  cycle from run to run. A change that makes the callback slower on purpose
  re-pins by the same rule. See documentation/design/software-in-the-loop.md,
  Part B3.

  @sil @REQ-PERF-004 @REQ-PERF-008
  Scenario Outline: The <current> current loop stays within its budget in <mode> mode
    Given a nominal motor plant
    And the plant response is recorded at 1000 Hz for up to 800 samples
    And the motor is already calibrated
    And the motor boots in <mode> mode
    And the current loop runs the <current> algorithm
    When the target boots
    And the control loop statistics are cleared
    And the rotor is aligned
    Then the current loop shall be running the <current> algorithm
    When <setpoint>
    And the motor is enabled
    And the response is captured for 300 ms after enable
    Then the control loop shall have stayed within its budget
    And no control loop execution shall have taken more than <max_cycles> cycles

    Examples:
      | mode     | current   | setpoint                                  | max_cycles |
      | torque   | pid       | a torque setpoint of 0.5 A is applied     | 160        |
      | torque   | decoupled | a torque setpoint of 0.5 A is applied     | 160        |
      | torque   | deadbeat  | a torque setpoint of 0.5 A is applied     | 160        |
      | torque   | sliding   | a torque setpoint of 0.5 A is applied     | 160        |
      | speed    | decoupled | a speed setpoint of 20 rad/s is applied   | 180        |
      | position | decoupled | a position setpoint of 1.5 rad is applied | 180        |

  @sil @REQ-PERF-004 @REQ-PERF-008
  Scenario: The control interrupt stays within its budget through a full calibration
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the control loop statistics are cleared
    And the full calibration is run from the terminal
    Then the control loop shall have stayed within its budget
    And no control loop execution shall have taken more than 360 cycles
