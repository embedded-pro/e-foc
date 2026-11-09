Feature: Position Travel
  Position mode reports its target reached once the position error has stayed within the
  position window for the window time (REQ-CTL-003). The setpoint is a single-turn angle: with
  no travel limits set the rotor takes the shorter way round to it (REQ-CTL-003). A host that sequences moves sends a
  setpoint and reads targetReached until it reads 1; a read that follows the setpoint is
  answered from the error against that setpoint, so it never carries the previous move's flag.
  The window defaults to four counts of the default 4000-count encoder and its time to 20 ms;
  a window of 0 keeps the target reported not reached, as does every mode but position.

  With a position minimum below the position maximum, position mode refuses a setpoint outside
  them and moves the direct way between them, never across the angle outside them, whichever is
  the shorter way round (REQ-CTL-007). The limits are measured from the alignment zero.
  A rotor that an outside force carries more than four encoder counts past them faults the drive
  with positionLimit (REQ-CTL-007); stall reporting is turned off so that only the limit can
  answer the hand that pushes the shaft.

  @sil @REQ-CTL-003 @REQ-COM-001
  Scenario: A position move reports its target reached once the rotor settles
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in position mode
    When the target boots
    And the rotor is aligned
    And a position setpoint of 0.0 rad is applied
    And the motor is enabled
    Then the targetReached read over CAN shall become 1 within 500 ms
    When a position setpoint of 1.5 rad is applied
    Then the targetReached read over CAN shall be 0
    And the targetReached read over CAN shall become 1 within 1500 ms

  @sil @REQ-CTL-003
  Scenario: With the position window off, a settled rotor is not reported reached
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in position mode
    When the target boots
    And the positionWindow is set to 0 over CAN
    And the rotor is aligned
    And a position setpoint of 0.0 rad is applied
    And the motor is enabled
    Then the targetReached read over CAN shall stay 0 for 1000 ms

  @sil @REQ-CTL-003
  Scenario: A running torque mode never reports a target reached
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a torque setpoint of 0 A is applied
    Then the targetReached read over CAN shall stay 0 for 1000 ms

  @sil @REQ-CTL-003 @REQ-COM-001
  Scenario: Without travel limits a move across the seam turns the shorter way round
    Given a nominal motor plant
    And the plant response is recorded at 100 Hz for up to 2000 samples
    And the motor is already calibrated
    And the motor boots in position mode
    When the target boots
    And the rotor is aligned
    And a position setpoint of 2.5 rad is applied
    And the motor is enabled
    Then the targetReached read over CAN shall become 1 within 1000 ms
    When a position setpoint of -2.5 rad is applied
    Then the targetReached read over CAN shall become 1 within 1100 ms
    When the response is captured for 3000 ms after the last setpoint
    Then the rotor shall travel less than 2.0 rad after the last setpoint

  @sil @REQ-CTL-007 @REQ-COM-001
  Scenario: Within the travel limits a move across the seam turns the direct way
    Given a nominal motor plant
    And the plant response is recorded at 100 Hz for up to 2000 samples
    And the motor is already calibrated
    And the motor boots in position mode
    When the target boots
    And the positionMinimum is set to -3.0 over CAN
    And the positionMaximum is set to 3.0 over CAN
    And the rotor is aligned
    And a position setpoint of 2.5 rad is applied
    And the motor is enabled
    Then the targetReached read over CAN shall become 1 within 1000 ms
    When a position setpoint of -2.5 rad is applied
    Then the targetReached read over CAN shall become 1 within 1100 ms
    When the response is captured for 3000 ms after the last setpoint
    Then the rotor shall travel at least 4.0 rad after the last setpoint

  @sil @REQ-CTL-007 @REQ-COM-001
  Scenario: A setpoint outside the travel limits is refused and the previous one stays in force
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in position mode
    When the target boots
    And the positionMinimum is set to -1.0 over CAN
    And the positionMaximum is set to 1.0 over CAN
    And the rotor is aligned
    And a position setpoint of 0.5 rad is applied
    And the motor is enabled
    Then the targetReached read over CAN shall become 1 within 1000 ms
    And a position setpoint of 1.5 rad shall be refused
    And the targetReached read over CAN shall stay 1 for 500 ms

  @sil @REQ-CTL-007 @REQ-COM-001
  Scenario: A rotor pushed past the travel limits faults the drive
    Given a nominal motor plant
    And a hand turns the shaft by 0.5 rad over 100 ms, 1000 ms after enable, and holds it
    And the motor is already calibrated
    And the motor boots in position mode
    When the target boots
    And the stallDetection is set to 0 over CAN
    And the positionMinimum is set to -1.0 over CAN
    And the positionMaximum is set to 1.0 over CAN
    And the rotor is aligned
    And a position setpoint of 0.9 rad is applied
    And the motor is enabled
    Then the state machine shall report a position-limit fault

  @sil @REQ-COM-001
  Scenario: The position window, its time and the travel limits are stored and read back after a restart
    Given a nominal motor plant
    When the target boots
    And the positionWindow is set to 0.02 over CAN
    And the positionWindowTime is set to 0.05 over CAN
    And the positionMinimum is set to -2.5 over CAN
    And the positionMaximum is set to 1.5 over CAN
    And the target restarts
    Then the positionWindow read over CAN shall be 0.02
    And the positionWindowTime read over CAN shall be 0.05
    And the positionMinimum read over CAN shall be -2.5
    And the positionMaximum read over CAN shall be 1.5
