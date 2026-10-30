Feature: Fault Supervision
  Effort without motion has two causes the drive can tell apart. An encoder that has stopped
  reporting cannot show the rotor moving against the drive; a rotor that an outside force pushes
  away from where the drive holds it, and then holds still, can, because the encoder saw the push.
  The first is a sensor fault, the second a stall (REQ-SM-028). The plant's hand grips the shaft
  for the second: it turns the shaft a set angle and then holds it, whatever the drive does.

  The supervision settings are one drive-wide set, stored in the configuration record and in force
  at once (REQ-SM-033). Stall reporting can be turned off; the sensor fault cannot.

  @sil @REQ-SM-028 @REQ-INT-013 @REQ-CAN-011
  Scenario: A shaft pushed away from the held position and held there is reported as a stall
    Given a nominal motor plant
    And a hand turns the shaft by 0.5 rad over 100 ms, 1000 ms after enable, and holds it
    And the motor is already calibrated
    And the motor boots in position mode
    When the target boots
    And the rotor is aligned
    And a position setpoint of 0.0 rad is applied
    And the motor is enabled
    Then the state machine shall report a stall fault

  @sil @REQ-SM-033
  Scenario: With stall reporting off, a held shaft leaves the drive running
    Given a nominal motor plant
    And the plant response is recorded at 100 Hz for up to 400 samples
    And a hand turns the shaft by 0.5 rad over 100 ms, 1000 ms after enable, and holds it
    And the motor is already calibrated
    And the motor boots in position mode
    When the target boots
    And the stallDetection is set to 0 over CAN
    And the rotor is aligned
    And a position setpoint of 0.0 rad is applied
    And the motor is enabled
    And the response is captured for 3000 ms after enable
    Then the state machine shall be in the running state

  @sil @REQ-SM-028 @REQ-SM-033
  Scenario: With stall reporting off, an encoder that freezes is still a sensor fault
    Given a nominal motor plant
    And the encoder freezes 200 ms after enable
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the stallDetection is set to 0 over CAN
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 20 rad/s is applied
    Then the state machine shall report a sensor fault

  @sil @REQ-SM-033 @REQ-CAN-011
  Scenario: The supervision settings are stored and read back after a restart
    Given a nominal motor plant
    When the target boots
    And the supervisionWindow is set to 2.5 over CAN
    And the stallDetection is set to 0 over CAN
    And the target restarts
    Then the supervisionWindow read over CAN shall be 2.5
    And the stallDetection read over CAN shall be 0
