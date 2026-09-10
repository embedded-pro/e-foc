Feature: FOC Motor Calibration Flow
  Calibration estimates pole-pairs, R/L, alignment, and optionally
  mechanical parameters, then persists the result to non-volatile memory.
  Speed and position modes additionally run mechanical identification.

  @sil @REQ-SM-003 @REQ-SM-004 @REQ-SM-011
  Scenario Outline: <label> calibration succeeds and transitions to Ready
    Given the <mode> motor system is initialised with no valid calibration data
    When the <mode> control mode is selected
    And the calibrate command is issued
    Then the state machine shall be in the Ready state

    Examples:
      | label    | mode     |
      | Torque   | torque   |
      | Speed    | speed    |
      | Position | position |
