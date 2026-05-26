Feature: FOC State Machine Lifecycle
  The FOC state machine controls the motor lifecycle from power-on through
  calibration, enabling, and fault handling.

  # REQ-SM-001: no valid NVM → starts in Idle
  Scenario: Motor starts in Idle state when no valid calibration data exists
    Given the system is initialised with no valid calibration data
    Then the state machine shall be in the Idle state

  # REQ-SM-010: valid NVM → transitions directly to Ready
  Scenario: Motor boots directly to Ready state when valid calibration data exists
    Given the system is initialised with valid calibration data
    Then the state machine shall be in the Ready state

  # REQ-SM-002: Idle → Calibrating
  Scenario: Calibrate command transitions state machine from Idle to Calibrating
    Given the system is initialised with no valid calibration data
    When the calibrate command is issued
    Then the state machine shall be in the Calibrating state

  # REQ-SM-006: Ready → Enabled
  Scenario: Enable command transitions state machine from Ready to Enabled
    Given the system is initialised with valid calibration data
    When the enable command is issued
    Then the state machine shall be in the Enabled state

  # REQ-SM-007: Enabled → Ready
  Scenario: Disable command transitions state machine from Enabled to Ready
    Given the system is initialised with valid calibration data
    And the enable command is issued
    When the disable command is issued
    Then the state machine shall be in the Ready state

  # REQ-SM-008: hardware fault → Fault
  Scenario: Hardware fault event transitions state machine to Fault state
    Given the system is initialised with valid calibration data
    And the enable command is issued
    When a hardware fault is raised by the platform
    Then the state machine shall be in the Fault state

  # REQ-SM-009: Fault → Idle via clear
  Scenario: Clear fault command transitions state machine from Fault to Idle
    Given the system is initialised with valid calibration data
    And the enable command is issued
    And a hardware fault is raised by the platform
    When the clear fault command is issued
    Then the state machine shall be in the Idle state

  # Forbidden transitions — enable from wrong states
  Scenario: Enable command is rejected when motor is in Idle state
    Given the system is initialised with no valid calibration data
    When the enable command is issued
    Then the state machine shall be in the Idle state

  Scenario: Enable command is rejected when motor is in Fault state
    Given the system is initialised with valid calibration data
    And the enable command is issued
    And a hardware fault is raised by the platform
    When the enable command is issued
    Then the state machine shall be in the Fault state

  # Forbidden transitions — disable from wrong states
  Scenario: Disable command is ignored when motor is in Ready state
    Given the system is initialised with valid calibration data
    When the disable command is issued
    Then the state machine shall be in the Ready state

  Scenario: Disable command is ignored when motor is in Idle state
    Given the system is initialised with no valid calibration data
    When the disable command is issued
    Then the state machine shall be in the Idle state

  Scenario: Disable command is ignored when motor is in Fault state
    Given the system is initialised with valid calibration data
    And the enable command is issued
    And a hardware fault is raised by the platform
    When the disable command is issued
    Then the state machine shall be in the Fault state

  # Forbidden transitions — calibrate from wrong states
  Scenario: Calibrate command is rejected while motor is Enabled
    Given the system is initialised with valid calibration data
    And the enable command is issued
    When the calibrate command is issued
    Then the state machine shall be in the Enabled state

  Scenario: Calibrate command is rejected while motor is in Fault state
    Given the system is initialised with valid calibration data
    And the enable command is issued
    And a hardware fault is raised by the platform
    When the calibrate command is issued
    Then the state machine shall be in the Fault state

  # Forbidden transitions — clear fault from wrong states
  Scenario: Clear fault command is ignored when motor is in Ready state
    Given the system is initialised with valid calibration data
    When the clear fault command is issued
    Then the state machine shall be in the Ready state

  Scenario: Clear fault command is ignored when motor is in Idle state
    Given the system is initialised with no valid calibration data
    When the clear fault command is issued
    Then the state machine shall be in the Idle state

  Scenario: Clear fault command is ignored when motor is Enabled
    Given the system is initialised with valid calibration data
    And the enable command is issued
    When the clear fault command is issued
    Then the state machine shall be in the Enabled state

  # Async-callback race guard — REQ-SM-011
  Scenario: Clear-calibration NVM completion after Enable does not return motor to Idle
    Given the system is initialised with valid calibration data
    When a clear-calibration command is issued with deferred NVM completion
    And the enable command is issued
    And the deferred NVM invalidation completes successfully
    Then the state machine shall be in the Enabled state

  Scenario: Clear-calibration NVM completion after Fault does not return motor to Idle
    Given the system is initialised with valid calibration data
    When a clear-calibration command is issued with deferred NVM completion
    And the enable command is issued
    And a hardware fault is raised by the platform
    And the deferred NVM invalidation completes successfully
    Then the state machine shall be in the Fault state
