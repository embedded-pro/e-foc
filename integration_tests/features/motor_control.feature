Feature: FOC Motor Control Modes
  The CAN electrical identification command estimates pole-pairs and R/L, aligns
  the rotor, and persists the result to non-volatile memory. Torque mode needs
  nothing further: it reaches Ready, enables, accepts a setpoint, and sustains the
  running state without faulting. Speed and position additionally require
  mechanical parameters, which this command does not identify, so they stay out
  of Ready, report an incomplete calibration and refuse to enable.

  @sil @REQ-SM-003 @REQ-SM-004 @REQ-SM-005 @REQ-SM-011
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

  @sil @REQ-SM-003 @REQ-SM-004 @REQ-SM-006 @REQ-SM-011
  Scenario: Speed mode refuses to run without mechanical calibration
    Given the speed motor system is initialised with no valid calibration data
    When the speed control mode is selected
    And the calibrate command is issued
    Then the state machine shall report incomplete calibration
    And the motor shall refuse to enable

  @sil @REQ-SM-003 @REQ-SM-004 @REQ-SM-007 @REQ-SM-011
  Scenario: Position mode refuses to run without mechanical calibration
    Given the position motor system is initialised with no valid calibration data
    When the position control mode is selected
    And the calibrate command is issued
    Then the state machine shall report incomplete calibration
    And the motor shall refuse to enable
