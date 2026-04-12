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
