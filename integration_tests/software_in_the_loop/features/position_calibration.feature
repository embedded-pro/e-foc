Feature: Position Controller Calibration Flow
  The position controller calibration sequence estimates pole pairs,
  resistance/inductance, encoder alignment, and mechanical parameters,
  then persists the result to non-volatile memory.

  # REQ-POS-001, REQ-SM-003, REQ-SM-004, REQ-SM-011
  Scenario: Full position calibration sequence succeeds and transitions to Ready
    Given the position motor system is initialised with no valid calibration data
    And calibration service expectations are configured
    When the calibrate command is issued
    And the pole-pairs estimation completes with 7 pole pairs
    And the resistance-inductance estimation completes with resistance 500 milliohm and inductance 1000 microhenry
    And the alignment estimation completes with offset 0 radians
    And the mechanical identification completes successfully
    Then the state machine shall be in the Ready state

  # REQ-SM-005
  Scenario: Position calibration fails when pole-pairs estimation returns no result
    Given the position motor system is initialised with no valid calibration data
    And calibration service expectations are configured
    When the calibrate command is issued
    And the pole-pairs estimation reports failure
    Then the state machine shall be in the Fault state
