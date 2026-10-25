Feature: Motor controller application on the connected motor
  The motor controller firmware runs against a real motor: a delta-wound, 8-pole (4 pole pairs) brushless motor
  with a 1000 line quadrature encoder and a free shaft carrying a light disk. Every scenario starts from a reset
  target, keeps the commanded current within the board limit, and ends with the motor disabled.

  @hil_motor @REQ-HIL-020
  Scenario: Electrical identification measures the connected motor
    Given the torque motor system is initialised with no valid calibration data
    When the torque control mode is selected
    And electrical identification is run
    Then the state machine shall be in the Ready state
    And the identified pole pairs shall be 4
    And the identified resistance shall be between 0.8 and 1.4 ohm
    And the identified inductance shall be between 0.7 and 1.5 mH

  @hil_motor @REQ-HIL-021
  Scenario: Speed calibration identifies the rotor inertia
    Given the speed motor system is initialised with no valid calibration data
    When the speed control mode is selected
    And the full calibration is run
    Then the state machine shall be in the Ready state
    And the identified pole pairs shall be 4
    And the identified resistance shall be between 0.8 and 1.4 ohm
    And the identified inertia shall be between 8 and 25 uNms2

  @hil_motor @REQ-HIL-024
  Scenario Outline: Torque mode accelerates the rotor in the commanded direction
    Given the torque motor system is initialised with no valid calibration data
    When the torque control mode is selected
    And electrical identification is run
    And the motor is enabled
    And a torque setpoint of <torque> A is applied
    Then the measured speed shall exceed <speed> rad/s within 1000 ms
    When a torque setpoint of 0 A is applied
    And the motor is disabled
    Then the state machine shall be in the Ready state

    Examples:
      | torque | speed |
      | 0.3    | 20    |
      | -0.3   | -20   |

  @hil_motor @REQ-HIL-022
  Scenario Outline: Speed loop tracks its setpoint with the <current> current loop and the <speed> speed loop
    Given the speed motor system is initialised with no valid calibration data
    When the speed control mode is selected
    And the full calibration is run
    And the current loop algorithm is set to <current>
    And the speed loop algorithm is set to <speed>
    And the motor is enabled
    And a speed setpoint of 30 rad/s is applied
    Then the measured speed shall settle at 30 rad/s within 4 rad/s in 3000 ms
    When a speed setpoint of -30 rad/s is applied
    Then the measured speed shall settle at -30 rad/s within 4 rad/s in 3000 ms
    When a speed setpoint of 0 rad/s is applied
    Then the measured speed shall settle at 0 rad/s within 4 rad/s in 3000 ms
    And the motor shall not report a fault
    When the motor is disabled
    Then the state machine shall be in the Ready state

    Examples:
      | current   | speed  |
      | pid       | pid    |
      | pid       | lqi    |
      | pid       | adrc   |
      | pid       | twodof |
      | decoupled | pid    |
      | decoupled | lqi    |
      | decoupled | adrc   |
      | decoupled | twodof |
      | deadbeat  | pid    |
      | deadbeat  | lqi    |
      | deadbeat  | adrc   |
      | deadbeat  | twodof |
      | sliding   | pid    |
      | sliding   | lqi    |
      | sliding   | adrc   |
      | sliding   | twodof |

  @hil_motor @REQ-HIL-023
  Scenario Outline: Position loop holds its setpoint with the <position> position loop
    Given the position motor system is initialised with no valid calibration data
    When the position control mode is selected
    And the full calibration is run
    And the position loop algorithm is set to <position>
    And the motor is enabled
    And a position setpoint of 1.0 rad is applied
    Then the measured position shall settle at 1.0 rad within 0.3 rad in 4000 ms
    When a position setpoint of -1.0 rad is applied
    Then the measured position shall settle at -1.0 rad within 0.3 rad in 4000 ms
    And the motor shall not report a fault
    When the motor is disabled
    Then the state machine shall be in the Ready state

    Examples:
      | position |
      | pid      |
      | cascadep |
      | lqr      |
      | lqi      |
      | twodof   |
