Feature: FOC Motor Control Modes
  The CAN electrical identification command estimates pole-pairs and R/L, aligns
  the rotor, and persists the result to non-volatile memory. Torque mode needs
  nothing further: it reaches Ready, enables, accepts a setpoint, and sustains the
  running state without faulting. Speed and position additionally require
  mechanical parameters, which this command does not identify, so they stay out
  of Ready, report an incomplete calibration and refuse to enable.

  The first Start after power-up measures the current sensors' offsets again at rest
  (REQ-CMS-003). A sensor that drifted a little since the calibration is corrected and
  the drive runs; one that drifted more than 50 mA refuses the Start, which asks for a
  new calibration.

  @sil @REQ-CMS-001 @REQ-CMS-004 @REQ-COM-001
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

  @sil @REQ-CMS-001 @REQ-LCY-001 @REQ-CMS-004
  Scenario: Speed mode refuses to run without mechanical calibration
    Given the speed motor system is initialised with no valid calibration data
    When the speed control mode is selected
    And the calibrate command is issued
    Then the state machine shall report incomplete calibration
    And the motor shall refuse to enable

  @sil @REQ-CMS-001 @REQ-LCY-001 @REQ-CMS-004
  Scenario: Position mode refuses to run without mechanical calibration
    Given the position motor system is initialised with no valid calibration data
    When the position control mode is selected
    And the calibrate command is issued
    Then the state machine shall report incomplete calibration
    And the motor shall refuse to enable

  @sil @REQ-CMS-003
  Scenario: A current sensor 20 mA from its calibrated offset still lets the first Start run the drive
    Given a nominal motor plant
    And the motor is already calibrated
    And the phase A current sensor has drifted by 0.02 A since the calibration
    And the motor boots in torque mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    Then the state machine shall be in the running state

  @sil @REQ-CMS-003
  Scenario: A current sensor 100 mA from its calibrated offset refuses the first Start
    Given a nominal motor plant
    And the motor is already calibrated
    And the phase A current sensor has drifted by 0.1 A since the calibration
    And the motor boots in torque mode
    When the target boots
    And the rotor is aligned
    Then the motor start shall be refused as a failed calibration
    And the state machine shall be in the Ready state
