Feature: Disconnected And Faulty Motor Wiring
  A motor whose phases carry no current produces no torque, so the rotor stays
  where it is however the drive is commanded. The healthy plant is run through
  the same steps so the comparison is like for like.

  An encoder that has stopped reporting is the opposite failure: the rotor is
  free to turn and the drive cannot see it. Alignment declares the rotor settled
  once its reading stops changing, which a frozen reading satisfies perfectly, so
  the drive aligns against a reading that means nothing and enables. What gives
  it away is the first request for motion: the drive pushes torque current and
  the reading does not move. The encoder plausibility monitor (REQ-SM-028)
  reports that as a sensor fault, whether the encoder was stuck from boot or
  froze while the motor was running.

  @sil @REQ-SM-008
  Scenario: A disconnected motor does not turn when commanded
    Given a disconnected motor plant
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 20 rad/s is applied
    Then the rotor shall not turn

  @sil @REQ-SM-008
  Scenario: A motor with one open phase still turns, degraded and undetected
    Given an open phase motor plant
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 20 rad/s is applied
    Then the rotor shall turn

  @sil @REQ-SM-008 @REQ-SM-028
  Scenario: An encoder that freezes while running is detected as a sensor fault
    Given a nominal motor plant
    And the encoder freezes 200 ms after enable
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 20 rad/s is applied
    Then the state machine shall report a sensor fault

  @sil @REQ-SM-004 @REQ-SM-008 @REQ-SM-028
  Scenario: A stuck encoder aligns and enables, and is detected as a sensor fault once the drive asks for motion
    Given a stuck encoder motor plant
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 20 rad/s is applied
    Then the state machine shall report a sensor fault

  @sil @REQ-SM-006 @REQ-INT-014
  Scenario: A healthy motor turns when commanded and reports its speed
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 20 rad/s is applied
    Then the rotor shall turn
    And the reported speed shall settle within 2 rad/s of 20 rad/s
