Feature: Disconnected And Faulty Motor Wiring
  A motor whose phases carry no current produces no torque, so the rotor stays
  where it is however the drive is commanded. The healthy plant is run through
  the same steps so the comparison is like for like.

  @sil @REQ-SM-008
  Scenario: A disconnected motor does not turn when commanded
    Given a disconnected motor plant
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 20 rps is applied
    Then the rotor shall not turn

  # Two remaining phases still produce torque, so a single open phase is a degraded running
  # condition rather than a dead motor, and nothing in the firmware currently detects it.
  @sil @REQ-SM-008
  Scenario: A motor with one open phase still turns, degraded and undetected
    Given an open phase motor plant
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 20 rps is applied
    Then the rotor shall turn

  @sil @REQ-SM-006
  Scenario: A healthy motor turns when commanded
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 20 rps is applied
    Then the rotor shall turn
