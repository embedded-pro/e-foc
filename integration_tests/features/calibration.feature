Feature: FOC Motor Calibration Flow
  The CAN electrical identification command estimates pole-pairs and R/L, aligns
  the rotor, and persists the result to non-volatile memory. Torque mode needs
  nothing further and reaches Ready. Speed and position additionally require
  mechanical parameters, which this command does not identify, so they stay out
  of Ready and report an incomplete calibration.

  @sil @REQ-SM-003 @REQ-SM-004 @REQ-SM-011
  Scenario: Torque calibration succeeds and transitions to Ready
    Given the torque motor system is initialised with no valid calibration data
    When the torque control mode is selected
    And the calibrate command is issued
    Then the state machine shall be in the Ready state

  @sil @REQ-SM-003 @REQ-SM-004 @REQ-SM-011
  Scenario Outline: <label> calibration stays incomplete without mechanical parameters
    Given the <mode> motor system is initialised with no valid calibration data
    When the <mode> control mode is selected
    And the calibrate command is issued
    Then the state machine shall report incomplete calibration

    Examples:
      | label    | mode     |
      | Speed    | speed    |
      | Position | position |
