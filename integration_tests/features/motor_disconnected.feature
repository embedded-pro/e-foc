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
  free to turn and the drive cannot see it. A frozen reading looks settled at
  once, so alignment first holds the field a quarter electrical turn ahead and
  then moves it to the alignment angle: the encoder must follow by the matching
  mechanical angle, and one stuck from boot is refused there as a sensor fault
  (REQ-SM-034). An encoder that freezes later is found while the drive runs
  (REQ-SM-028): in speed and position mode the drive pushes torque current and
  the reading does not move; in any mode, torque mode included, a reading that
  stops dead faster than the rotor could physically stop is a sensor fault.

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

  @sil @REQ-SM-034
  Scenario: A stuck encoder fails its alignment as a sensor fault
    Given a stuck encoder motor plant
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor alignment is attempted
    Then the state machine shall report a sensor fault

  @sil @REQ-SM-008 @REQ-SM-028
  Scenario: An encoder that freezes while a torque-mode drive turns the rotor is detected as a sensor fault
    Given a bench motor plant
    And the encoder freezes 2000 ms after enable
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a torque setpoint of 0.3 A is applied
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
