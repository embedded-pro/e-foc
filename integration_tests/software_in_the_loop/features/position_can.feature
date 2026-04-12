Feature: Position Controller CAN Commands
  CAN commands received by the FocMotorCategoryServer are translated into
  state machine lifecycle transitions for the position controller.

  Background:
    Given the position motor system is initialised with valid calibration data
    And the CAN category server is connected to the state machine

  # REQ-INT-001
  Scenario: CAN Start command enables the position motor
    When the CAN Start command is received
    Then the state machine shall be in the Enabled state

  # REQ-INT-002
  Scenario: CAN Stop command disables the position motor
    Given the CAN Start command is received
    When the CAN Stop command is received
    Then the state machine shall be in the Ready state

  # REQ-INT-003
  Scenario: CAN ClearFault command clears the position controller fault state
    And the enable command is issued
    And a hardware fault is raised by the platform
    When the CAN ClearFault command is received
    Then the state machine shall be in the Idle state
