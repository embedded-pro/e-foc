Feature: Position Travel
  Position mode reports its target reached once the position error has stayed within the
  position window for the window time (REQ-POS-014). A host that sequences moves sends a
  setpoint and reads targetReached until it reads 1; a read that follows the setpoint is
  answered from the error against that setpoint, so it never carries the previous move's flag.
  The window defaults to four counts of the default 4000-count encoder and its time to 20 ms;
  a window of 0 keeps the target reported not reached, as does every mode but position.

  @sil @REQ-POS-014 @REQ-CAN-016
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

  @sil @REQ-POS-014
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

  @sil @REQ-POS-014
  Scenario: A running torque mode never reports a target reached
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a torque setpoint of 0 A is applied
    Then the targetReached read over CAN shall stay 0 for 1000 ms

  @sil @REQ-CAN-016
  Scenario: The position window and its time are stored and read back after a restart
    Given a nominal motor plant
    When the target boots
    And the positionWindow is set to 0.02 over CAN
    And the positionWindowTime is set to 0.05 over CAN
    And the target restarts
    Then the positionWindow read over CAN shall be 0.02
    And the positionWindowTime read over CAN shall be 0.05
