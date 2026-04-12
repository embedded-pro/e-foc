Feature: Position Controller Functional API
  The position controller exposes a typed API for setpoints and PID tunings
  across three cascade stages. These scenarios verify requirement-level acceptance.

  # REQ-POS-002: position setpoint acceptance
  Scenario: Position controller accepts a target position in mechanical radians
    Given the position controller is initialised with default parameters
    When a position setpoint of 3.14 radians is commanded
    Then the position setpoint is accepted without error

  # REQ-POS-003: independent per-loop tunings
  Scenario: Position controller accepts independent tunings for each cascade stage
    Given the position controller is initialised with default parameters
    When position current tunings kp=1.0 ki=0.1 kd=0 and q-axis kp=2.0 ki=0.2 kd=0 are configured
    And speed PID gains kp=5.0 ki=0.5 kd=0.01 are configured
    And position PID gains kp=10.0 ki=0.0 kd=0.1 are configured
    Then all three sets of tunings are stored independently

  # REQ-POS-004: position PID acceptance
  Scenario: Position controller accepts position loop PID gains
    Given the position controller is initialised with default parameters
    When position PID gains kp=10.0 ki=0.0 kd=0.1 are configured
    Then the position PID tunings are accepted without error

