Feature: Speed Controller State Machine Lifecycle
  The speed controller state machine manages the motor lifecycle,
  supporting cascade velocity control from Idle through calibration to Enabled.

  # REQ-SPD-001, REQ-SM-001
  Scenario: Speed motor starts in Idle state when no valid calibration data exists
    Given the speed motor system is initialised with no valid calibration data
    Then the state machine shall be in the Idle state

  # REQ-SPD-001, REQ-SM-010
  Scenario: Speed motor boots directly to Ready state when valid calibration data exists
    Given the speed motor system is initialised with valid calibration data
    Then the state machine shall be in the Ready state

  # REQ-SM-002
  Scenario: Calibrate command transitions speed controller from Idle to Calibrating
    Given the speed motor system is initialised with no valid calibration data
    When the calibrate command is issued
    Then the state machine shall be in the Calibrating state

  # REQ-SM-006
  Scenario: Enable command transitions speed controller from Ready to Enabled
    Given the speed motor system is initialised with valid calibration data
    When the enable command is issued
    Then the state machine shall be in the Enabled state

  # REQ-SM-007
  Scenario: Disable command transitions speed controller from Enabled to Ready
    Given the speed motor system is initialised with valid calibration data
    And the enable command is issued
    When the disable command is issued
    Then the state machine shall be in the Ready state

  # REQ-SM-008
  Scenario: Hardware fault transitions speed controller to Fault state
    Given the speed motor system is initialised with valid calibration data
    And the enable command is issued
    When a hardware fault is raised by the platform
    Then the state machine shall be in the Fault state

  # REQ-SM-009
  Scenario: Clear fault command transitions speed controller from Fault to Idle
    Given the speed motor system is initialised with valid calibration data
    And the enable command is issued
    And a hardware fault is raised by the platform
    When the clear fault command is issued
    Then the state machine shall be in the Idle state
