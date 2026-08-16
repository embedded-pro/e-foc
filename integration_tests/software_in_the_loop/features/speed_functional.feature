Feature: Speed Controller Functional API
  The speed controller exposes a typed API for setpoints and loop bandwidths.
  These scenarios verify requirement-level acceptance of each configuration call.

  @REQ-SPD-002
  Scenario: Speed controller accepts a velocity setpoint in radians per second
    Given the speed controller is initialised with default parameters
    When a velocity setpoint of 100 radians per second is commanded
    Then the commanded duty cycles follow the velocity setpoint

  @REQ-SPD-003
  Scenario: Speed controller applies the configured current loop bandwidth
    Given the speed controller is initialised with default parameters
    When a current loop bandwidth an order of magnitude below the default is configured
    Then the commanded duty cycles differ from those of the default bandwidth

  @REQ-SPD-004
  Scenario: Speed controller accepts an outer velocity loop bandwidth and reports its frequency
    Given the speed controller is initialised with default parameters
    When the speed loop bandwidth is configured
    Then the outer loop frequency is 1000 Hz
