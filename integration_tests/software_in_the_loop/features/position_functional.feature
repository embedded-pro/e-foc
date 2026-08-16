Feature: Position Controller Functional API
  The position controller exposes a typed API for setpoints and loop bandwidths
  across three cascade stages. These scenarios verify requirement-level acceptance.

  @REQ-POS-002
  Scenario: Position controller accepts a target position in mechanical radians
    Given the position controller is initialised with default parameters
    When a position setpoint of 3.14 radians is commanded
    Then the commanded duty cycles follow the position setpoint

  @REQ-POS-003
  Scenario: Position controller accepts an independent bandwidth for each cascade stage
    Given the position controller is initialised with default parameters
    When the position current loop bandwidth is configured
    And the cascade speed loop bandwidth is configured
    And the position loop bandwidth is configured
    Then each configured bandwidth acts on its own loop

  @REQ-POS-004
  Scenario: Position controller applies the configured position loop bandwidth
    Given the position controller is initialised with default parameters
    When the position loop bandwidth is configured
    Then the commanded duty cycles differ from those of the detuned position loop

