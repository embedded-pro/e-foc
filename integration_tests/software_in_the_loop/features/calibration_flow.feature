Feature: FOC Motor Calibration Flow
  The calibration sequence estimates pole pairs, resistance/inductance, and
  encoder alignment offset, then persists the result to non-volatile memory.

  # REQ-SM-003, REQ-SM-004, REQ-SM-011
  Scenario: Full calibration sequence succeeds and transitions state machine to Ready
    Given the system is initialised with no valid calibration data
    And calibration service expectations are configured
    When the calibrate command is issued
    And the pole-pairs estimation completes with 7 pole pairs
    And the resistance-inductance estimation completes with resistance 500 milliohm and inductance 1000 microhenry
    And the alignment estimation completes with offset 0 radians
    Then the state machine shall be in the Ready state

  # REQ-SM-005: calibration failure → Fault
  Scenario: Calibration fails when pole-pairs estimation returns no result
    Given the system is initialised with no valid calibration data
    And calibration service expectations are configured
    When the calibrate command is issued
    And the pole-pairs estimation reports failure
    Then the state machine shall be in the Fault state
