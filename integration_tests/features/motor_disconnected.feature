Feature: Disconnected And Faulty Motor Wiring
  A motor whose phases carry no current produces no torque, so the rotor stays
  where it is however the drive is commanded, and a rotor that does not move is
  exactly what alignment waits for. The alignment therefore checks that the vector
  it applies drives its current through every phase before it zeroes the encoder,
  and reports a missing phase when one does not (REQ-EH-016). A cable pulled while
  the drive runs is found by the phase-loss monitor: the drive demands current it
  cannot make flow while the rotor is slow. The healthy plant is run through the
  same steps so the comparison is like for like.

  An encoder that has stopped reporting is the opposite failure: the rotor is
  free to turn and the drive cannot see it. Alignment declares the rotor settled
  once its reading stops changing, which a frozen reading satisfies perfectly, so
  the drive aligns against a reading that means nothing and enables. What gives
  it away is the first request for motion: the drive pushes torque current and
  the reading does not move. The encoder plausibility monitor (REQ-SM-028)
  reports that as a sensor fault, whether the encoder was stuck from boot or
  froze while the motor was running.

  @sil @REQ-EH-016 @REQ-CAN-014
  Scenario: A disconnected motor fails its alignment as a missing phase
    Given a disconnected motor plant
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor alignment is attempted
    Then the state machine shall report a missing-phase fault

  @sil @REQ-EH-016
  Scenario: A motor with one open phase fails its alignment as a missing phase
    Given an open phase motor plant
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor alignment is attempted
    Then the state machine shall report a missing-phase fault

  @sil @REQ-EH-016
  Scenario: A healthy motor aligns and turns through the same steps
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 20 rad/s is applied
    Then the rotor shall turn
    And the state machine shall be in the running state

  @sil @REQ-EH-016 @REQ-CAN-014
  Scenario: A cable pulled while the drive runs is reported as a missing phase
    Given a nominal motor plant
    And the motor cable is pulled 300 ms after enable
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 5 rad/s is applied
    Then the state machine shall report a missing-phase fault

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

  @sil @REQ-SM-028
  Scenario Outline: A position held against a shaft load after a full calibration is not reported under the <position> law
    Given a bench motor plant
    And the plant response is recorded at 100 Hz for up to 2000 samples
    And a torque step of 0.01 Nm applied 6000 ms after enable
    And the motor boots in position mode
    And the position loop runs the <position> algorithm
    When the target boots
    And the full calibration is run
    And a position setpoint of 0.0 rad is applied
    And the motor is enabled
    And the response is captured for 2500 ms after enable
    And a position setpoint of 0.0 rad is applied
    And the response is captured for 5000 ms after enable
    And a position setpoint of 0.0 rad is applied
    And the response is captured for 7500 ms after enable
    And a position setpoint of 0.0 rad is applied
    And the response is captured for 9000 ms after enable
    Then the position loop shall be running the <position> algorithm
    And the state machine shall be in the running state

    Examples:
      | position |
      | lqr      |
      | lqi      |

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
