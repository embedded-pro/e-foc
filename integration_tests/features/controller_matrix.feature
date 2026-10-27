Feature: FOC Controller Algorithm Coverage
  Each control loop can run any of its algorithms. Every algorithm is selected
  through the stored configuration against the nominal plant, and the firmware is
  asked which one actually took effect, because a state feedback design that does
  not converge leaves the previous algorithm running rather than failing loudly.
  The combinations below cover every current, speed and position algorithm at
  least once; the default PID current loop is covered by the control performance
  scenarios, and each law's closed-loop envelope by the host model-in-the-loop
  tests.

  @sil @REQ-CTRL-001
  Scenario Outline: <label> combines a <current> current loop with the outer loops
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in <mode> mode
    And the current loop runs the <current> algorithm
    And the speed loop runs the <speed> algorithm
    And the position loop runs the <position> algorithm
    When the target boots
    And the rotor is aligned
    Then the current loop shall be running the <current> algorithm
    And the speed loop shall be running the <speed> algorithm
    And the position loop shall be running the <position> algorithm
    When the motor is enabled
    And <setpoint>
    Then the state machine shall be in the running state
    When the motor is disabled
    Then the state machine shall be in the Ready state

    Examples:
      | label             | mode     | current   | speed  | position | setpoint                                  |
      | Deadbeat speed    | speed    | deadbeat  | lqi    | pid      | a speed setpoint of 20 rad/s is applied   |
      | Sliding speed     | speed    | sliding   | adrc   | pid      | a speed setpoint of 20 rad/s is applied   |
      | Deadbeat position | position | deadbeat  | pid    | lqr      | a position setpoint of 1.5 rad is applied |
      | Decoupled two-DOF | position | decoupled | twodof | twodof   | a position setpoint of 1.5 rad is applied |
      | Sliding LQI       | position | sliding   | lqi    | lqi      | a position setpoint of 1.5 rad is applied |
      | Decoupled cascade | position | decoupled | adrc   | cascadep | a position setpoint of 1.5 rad is applied |
