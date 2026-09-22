Feature: Disconnected And Faulty Motor Wiring
  A motor whose phases carry no current produces no torque, so the rotor stays
  where it is however the drive is commanded. The healthy plant is run through
  the same steps so the comparison is like for like.

  An encoder that has stopped reporting is the opposite failure: the rotor is
  free to turn and the drive cannot see it. Alignment declares the rotor settled
  once its reading stops changing, which a frozen reading satisfies perfectly, so
  the drive aligns against a reading that means nothing and enables. Nothing
  afterwards compares the reading against the current it is pushing, so the loop
  regulates a position that never moves. The @sil scenario below records what the
  product does today; the @sil-known-defect scenario carries what it should do.

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

  @sil @REQ-SM-004 @REQ-SM-008
  Scenario: A stuck encoder aligns and enables, and the drive sees a rotor that never moves
    Given a stuck encoder motor plant
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 20 rad/s is applied
    Then the state machine shall be in the running state
    And the rotor shall not turn

  @sil-known-defect @REQ-SM-008
  Scenario: A stuck encoder shall be detected as a sensor fault
    Given a stuck encoder motor plant
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 20 rad/s is applied
    Then the state machine shall report an sensor fault

  @sil @REQ-SM-006
  Scenario: A healthy motor turns when commanded
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 20 rad/s is applied
    Then the rotor shall turn
