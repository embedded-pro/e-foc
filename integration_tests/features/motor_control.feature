Feature: FOC Motor Control Modes
  After calibration, each control mode enables the motor, accepts a
  setpoint, and sustains the running state without faulting.

  @sil @REQ-SM-005
  Scenario: Torque mode runs motor with current setpoint
    Given the torque motor system is initialised with no valid calibration data
    When the torque control mode is selected
    And the calibrate command is issued
    Then the state machine shall be in the Ready state
    When the motor is enabled
    And a torque setpoint of 0.5 A is applied
    Then the state machine shall be in the running state
    When the motor is disabled
    Then the state machine shall be in the Ready state

  @sil @REQ-SM-006
  Scenario: Speed mode runs motor with speed setpoint
    Given the speed motor system is initialised with no valid calibration data
    When the speed control mode is selected
    And the calibrate command is issued
    Then the state machine shall be in the Ready state
    When the motor is enabled
    And a speed setpoint of 10.0 rps is applied
    Then the state machine shall be in the running state
    When the motor is disabled
    Then the state machine shall be in the Ready state

  @sil @REQ-SM-007
  Scenario: Position mode runs motor with position setpoint
    Given the position motor system is initialised with no valid calibration data
    When the position control mode is selected
    And the calibrate command is issued
    Then the state machine shall be in the Ready state
    When the motor is enabled
    And a position setpoint of 3.14 rad is applied
    Then the state machine shall be in the running state
    When the motor is disabled
    Then the state machine shall be in the Ready state
