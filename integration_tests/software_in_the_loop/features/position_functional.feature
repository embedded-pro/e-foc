Feature: Position Controller Functional API
  The position controller exposes a typed API for setpoints and loop bandwidths
  across three cascade stages. These scenarios verify requirement-level acceptance.

  @REQ-POS-002
  Scenario: Position controller accepts a target position in mechanical radians
    Given the position controller is initialised with default parameters
    When a position setpoint of 3.14 radians is commanded
    Then the position setpoint is accepted without error

  @REQ-POS-003
  Scenario: Position controller accepts an independent bandwidth for each cascade stage
    Given the position controller is initialised with default parameters
    When the position current loop bandwidth is configured
    And the cascade speed loop bandwidth is configured
    And the position loop bandwidth is configured
    Then all three loop bandwidths are stored independently

  @REQ-POS-004
  Scenario: Position controller accepts a position loop bandwidth
    Given the position controller is initialised with default parameters
    When the position loop bandwidth is configured
    Then the position loop bandwidth is accepted without error

