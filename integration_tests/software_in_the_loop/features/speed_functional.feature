Feature: Speed Controller Functional API
  The speed controller exposes a typed API for setpoints and PID tunings.
  These scenarios verify requirement-level acceptance of each configuration call.

  # REQ-SPD-002: setpoint acceptance
  Scenario: Speed controller accepts a velocity setpoint in radians per second
    Given the speed controller is initialised with default parameters
    When a velocity setpoint of 100 radians per second is commanded
    Then the setpoint is accepted without error

  # REQ-SPD-003: independent d/q current tunings
  Scenario: Speed controller accepts independent d-axis and q-axis current tunings
    Given the speed controller is initialised with default parameters
    When d-axis gains kp=1.0 ki=0.1 kd=0 and q-axis gains kp=2.0 ki=0.2 kd=0 are configured
    Then the d-axis and q-axis current tunings are stored independently

  # REQ-SPD-004: speed PID acceptance and outer loop frequency
  Scenario: Speed controller accepts outer velocity loop PID gains and reports its frequency
    Given the speed controller is initialised with default parameters
    When the speed PID gains kp=5.0 ki=0.5 kd=0.01 are configured
    Then the outer loop frequency is 1000 Hz
