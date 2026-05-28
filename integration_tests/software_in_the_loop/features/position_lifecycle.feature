Feature: Position Controller State Machine Lifecycle
  The position controller state machine manages the motor lifecycle,
  supporting triple-cascade position control from Idle through calibration to Enabled.

  # REQ-POS-001, REQ-SM-001
  Scenario: Position motor starts in Idle state when no valid calibration data exists
    Given the position motor system is initialised with no valid calibration data
    Then the state machine shall be in the Idle state

  # REQ-POS-001, REQ-SM-010
  Scenario: Position motor boots directly to Ready state when valid calibration data exists
    Given the position motor system is initialised with valid calibration data
    Then the state machine shall be in the Ready state

  # REQ-SM-002
  Scenario: Calibrate command transitions position controller from Idle to Calibrating
    Given the position motor system is initialised with no valid calibration data
    When the calibrate command is issued
    Then the state machine shall be in the Calibrating state

  # REQ-SM-006
  Scenario: Enable command transitions position controller from Ready to Enabled
    Given the position motor system is initialised with valid calibration data
    When the enable command is issued
    Then the state machine shall be in the Enabled state

  # REQ-SM-007
  Scenario: Disable command transitions position controller from Enabled to Ready
    Given the position motor system is initialised with valid calibration data
    And the enable command is issued
    When the disable command is issued
    Then the state machine shall be in the Ready state

  # REQ-SM-008
  Scenario: Hardware fault transitions position controller to Fault state
    Given the position motor system is initialised with valid calibration data
    And the enable command is issued
    When a hardware fault is raised by the platform
    Then the state machine shall be in the Fault state

  # REQ-SM-009
  Scenario: Clear fault command transitions position controller from Fault to Idle
    Given the position motor system is initialised with valid calibration data
    And the enable command is issued
    And a hardware fault is raised by the platform
    When the clear fault command is issued
    Then the state machine shall be in the Idle state

  # Forbidden transitions — enable from wrong states
  Scenario: Enable command is rejected when position motor is in Idle state
    Given the position motor system is initialised with no valid calibration data
    When the enable command is issued
    Then the state machine shall be in the Idle state

  Scenario: Enable command is rejected when position motor is in Fault state
    Given the position motor system is initialised with valid calibration data
    And the enable command is issued
    And a hardware fault is raised by the platform
    When the enable command is issued
    Then the state machine shall be in the Fault state

  # Forbidden transitions — disable from wrong states
  Scenario: Disable command is ignored when position motor is in Ready state
    Given the position motor system is initialised with valid calibration data
    When the disable command is issued
    Then the state machine shall be in the Ready state

  Scenario: Disable command is ignored when position motor is in Idle state
    Given the position motor system is initialised with no valid calibration data
    When the disable command is issued
    Then the state machine shall be in the Idle state

  Scenario: Disable command is ignored when position motor is in Fault state
    Given the position motor system is initialised with valid calibration data
    And the enable command is issued
    And a hardware fault is raised by the platform
    When the disable command is issued
    Then the state machine shall be in the Fault state

  # Forbidden transitions — calibrate from wrong states
  Scenario: Calibrate command is rejected while position motor is Enabled
    Given the position motor system is initialised with valid calibration data
    And the enable command is issued
    When the calibrate command is issued
    Then the state machine shall be in the Enabled state

  Scenario: Calibrate command is rejected while position motor is in Fault state
    Given the position motor system is initialised with valid calibration data
    And the enable command is issued
    And a hardware fault is raised by the platform
    When the calibrate command is issued
    Then the state machine shall be in the Fault state

  # Forbidden transitions — clear fault from wrong states
  Scenario: Clear fault command is ignored when position motor is in Ready state
    Given the position motor system is initialised with valid calibration data
    When the clear fault command is issued
    Then the state machine shall be in the Ready state

  Scenario: Clear fault command is ignored when position motor is in Idle state
    Given the position motor system is initialised with no valid calibration data
    When the clear fault command is issued
    Then the state machine shall be in the Idle state

  Scenario: Clear fault command is ignored when position motor is Enabled
    Given the position motor system is initialised with valid calibration data
    And the enable command is issued
    When the clear fault command is issued
    Then the state machine shall be in the Enabled state

  # Async-callback race guard — REQ-SM-011
  Scenario: Clear-calibration NVM completion after Enable does not return position motor to Idle
    Given the position motor system is initialised with valid calibration data
    When a clear-calibration command is issued with deferred NVM completion
    And the enable command is issued
    And the deferred NVM invalidation completes successfully
    Then the state machine shall be in the Enabled state

  Scenario: Clear-calibration NVM completion after Fault does not return position motor to Idle
    Given the position motor system is initialised with valid calibration data
    When a clear-calibration command is issued with deferred NVM completion
    And the enable command is issued
    And a hardware fault is raised by the platform
    And the deferred NVM invalidation completes successfully
    Then the state machine shall be in the Fault state
