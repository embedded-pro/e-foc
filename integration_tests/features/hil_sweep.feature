Feature: Controller sweep on the connected motor
  DIAG ONLY, not for merge. Runs every controller combination on the rig and prints
  settle, overshoot and hold metrics for each, inside the board's current limit.

  @hil_sweep
  Scenario Outline: Torque mode with the <current> current loop at <torque> A
    Given the torque motor system is initialised with no valid calibration data
    When the torque control mode is selected
    And electrical identification is run
    And the current loop algorithm is set to <current>
    And the motor is enabled
    And a torque setpoint of <torque> A is applied
    Then the measured speed shall exceed <speed> rad/s within 1000 ms
    When a torque setpoint of 0 A is applied
    And the motor is disabled
    Then the state machine shall be in the Ready state

    Examples:
      | current   | torque | speed |
      | pid       | 0.3    | 20    |
      | pid       | -0.3   | -20   |
      | decoupled | 0.3    | 20    |
      | decoupled | -0.3   | -20   |
      | deadbeat  | 0.3    | 20    |
      | deadbeat  | -0.3   | -20   |
      | sliding   | 0.3    | 20    |
      | sliding   | -0.3   | -20   |

  @hil_sweep
  Scenario Outline: Speed mode with the <current> current loop and the <speed> speed loop
    Given the speed motor system is initialised with no valid calibration data
    When the speed control mode is selected
    And the full calibration is run from the terminal
    And the current loop algorithm is set to <current>
    And the speed loop algorithm is set to <speed>
    And the motor is enabled
    And a speed setpoint of 30 rad/s is applied
    Then the measured speed shall settle at 30 rad/s within 4 rad/s in 3000 ms
    And the measured speed shall hold at 30 rad/s within 4 rad/s for 2000 ms
    When a speed setpoint of 60 rad/s is applied
    Then the measured speed shall settle at 60 rad/s within 4 rad/s in 3000 ms
    And the measured speed shall hold at 60 rad/s within 4 rad/s for 2000 ms
    When a speed setpoint of -30 rad/s is applied
    Then the measured speed shall settle at -30 rad/s within 4 rad/s in 3000 ms
    And the measured speed shall hold at -30 rad/s within 4 rad/s for 2000 ms
    When a speed setpoint of 0 rad/s is applied
    Then the measured speed shall settle at 0 rad/s within 4 rad/s in 3000 ms
    And the measured speed shall hold at 0 rad/s within 4 rad/s for 2000 ms
    And the motor shall not report a fault
    When the online estimates are read
    And the motor is disabled
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

  @hil_sweep @hil_sweep_position
  Scenario Outline: Position mode with the <position> position loop
    Given the position motor system is initialised with no valid calibration data
    When the position control mode is selected
    And the full calibration is run from the terminal
    And the position loop algorithm is set to <position>
    And the motor is enabled
    And a position setpoint of 1.0 rad is applied
    Then the measured position shall settle at 1.0 rad within 0.3 rad in 4000 ms
    And the measured position shall settle at 1.0 rad within 0.03 rad in 4000 ms
    And the measured position shall hold at 1.0 rad within 0.3 rad for 2000 ms
    When a position setpoint of -1.0 rad is applied
    Then the measured position shall settle at -1.0 rad within 0.3 rad in 4000 ms
    And the measured position shall settle at -1.0 rad within 0.03 rad in 4000 ms
    And the measured position shall hold at -1.0 rad within 0.3 rad for 2000 ms
    When a position setpoint of 3.0 rad is applied
    Then the measured position shall settle at 3.0 rad within 0.3 rad in 4000 ms
    And the measured position shall settle at 3.0 rad within 0.03 rad in 4000 ms
    And the measured position shall hold at 3.0 rad within 0.3 rad for 2000 ms
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

  @hil_breakaway
  Scenario: Breakaway current around the shaft
    Given the torque motor system is initialised with no valid calibration data
    When the torque control mode is selected
    And electrical identification is run
    And the motor is enabled
    Then the breakaway current is mapped over 24 trials in steps of 0.005 A up to 0.3 A held for 150 ms
    When the motor is disabled
    Then the state machine shall be in the Ready state

  @hil_hunt_trace
  Scenario Outline: The <position> position loop holds, traced
    Given the position motor system is initialised with no valid calibration data
    When the position control mode is selected
    And the full calibration is run from the terminal
    And the position loop algorithm is set to <position>
    And the motor is enabled
    And a position setpoint of 1.0 rad is applied
    Then the measured position shall settle at 1.0 rad within 0.3 rad in 4000 ms
    And the measured position and speed are traced for 3000 ms
    When a position setpoint of -1.0 rad is applied
    Then the measured position shall settle at -1.0 rad within 0.3 rad in 4000 ms
    And the measured position and speed are traced for 3000 ms
    And the motor shall not report a fault
    When the motor is disabled
    Then the state machine shall be in the Ready state

    Examples:
      | position |
      | pid      |
      | cascadep |
