Feature: CAN FOC Motor Category Commands
  CAN commands received by the FocMotorCategoryServer are translated into
  state machine lifecycle transitions via the FocMotorStateMachineBridge.

  Background:
    Given the system is initialised with valid calibration data
    And the CAN category server is connected to the state machine

  # REQ-INT-001
  Scenario: CAN Start command enables the motor
    When the CAN Start command is received
    Then the state machine shall be in the Enabled state

  # REQ-INT-002
  Scenario: CAN Stop command disables the motor
    Given the CAN Start command is received
    When the CAN Stop command is received
    Then the state machine shall be in the Ready state

  # REQ-INT-003
  Scenario: CAN ClearFault command clears the fault state
    And the enable command is issued
    And a hardware fault is raised by the platform
    When the CAN ClearFault command is received
    Then the state machine shall be in the Idle state

  # Forbidden CAN transitions
  Scenario: CAN Stop command is ignored when motor is already in Ready state
    When the CAN Stop command is received
    Then the state machine shall be in the Ready state

  Scenario: CAN ClearFault command is ignored when motor is not in Fault state
    When the CAN ClearFault command is received
    Then the state machine shall be in the Ready state

  Scenario: CAN Start then Stop then Start leaves motor stably in Enabled
    When the CAN Start command is received
    And the CAN Stop command is received
    And the CAN Start command is received
    Then the state machine shall be in the Enabled state

  # REQ-INT-004: CAN Start rejected when motor is already in Fault state
  Scenario: CAN Start command is rejected when motor is in Fault state
    And the enable command is issued
    And a hardware fault is raised by the platform
    When the CAN Start command is received
    Then the state machine shall be in the Fault state

  # REQ-INT-005: CAN Start rejected when motor is in Idle state
  Scenario: CAN Start command is rejected when motor is in Idle state
    And the enable command is issued
    And a hardware fault is raised by the platform
    And the CAN ClearFault command is received
    When the CAN Start command is received
    Then the state machine shall be in the Idle state

  # REQ-INT-006: CAN EmergencyStop in Enabled returns motor to Ready
  Scenario: CAN EmergencyStop in Enabled returns motor to Ready
    And the CAN Start command is received
    When the CAN EmergencyStop command is received
    Then the state machine shall be in the Ready state

  # REQ-INT-007: CAN EmergencyStop in Ready keeps motor in Ready
  Scenario: CAN EmergencyStop in Ready keeps motor in Ready
    When the CAN EmergencyStop command is received
    Then the state machine shall be in the Ready state
