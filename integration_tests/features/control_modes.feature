Feature: FOC Control Modes Against a Simulated Plant
  With the motor characteristics supplied to the target at boot and a complete
  calibration restored from non-volatile memory, each control mode aligns the
  rotor, enables, accepts a setpoint, and sustains the running state.

  @sil @REQ-SM-005 @REQ-SM-006 @REQ-SM-007
  Scenario Outline: <label> mode runs against the nominal plant
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in <mode> mode
    When the target boots
    And the rotor is aligned
    Then the state machine shall be in the Ready state
    When the motor is enabled
    And <setpoint>
    Then the state machine shall be in the running state
    When the motor is disabled
    Then the state machine shall be in the Ready state

    Examples:
      | label    | mode     | setpoint                                     |
      | Torque   | torque   | a torque setpoint of 0.5 A is applied        |
      | Speed    | speed    | a speed setpoint of 20 rad/s is applied        |
      | Position | position | a position setpoint of 1.5 rad is applied    |
