Feature: FOC Control Modes Against a Simulated Plant
  With the motor characteristics supplied to the target at boot and a complete
  calibration restored from non-volatile memory, a switch to another mode keeps
  the alignment the previous mode established. That each mode aligns, enables,
  runs and stops on the nominal plant is covered by the control performance
  scenarios.

  @sil @REQ-SM-032
  Scenario: A mode switch out of an aligned Ready runs without a new alignment
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the rotor is aligned
    And the speed control mode is selected
    And the motor is enabled
    And a speed setpoint of 20 rad/s is applied
    Then the state machine shall be in the running state
    When the motor is disabled
    Then the state machine shall be in the Ready state

  @sil @REQ-SM-032
  Scenario: A mode switch out of an unaligned Idle still needs an alignment
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the speed control mode is selected
    Then the motor shall refuse to enable
