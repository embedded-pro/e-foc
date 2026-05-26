Feature: CAN Control Mode Selection and Typed Setpoints
    # Covers: REQ-INT-CM-001 .. REQ-INT-CM-005

    Background:
        Given the multi-mode system is initialised with valid calibration data
        And the CAN category server is connected to the control mode coordinator

    # REQ-INT-CM-001 ---------------------------------------------------------------
    Scenario: SelectControlMode switches the active controller
        Given the active control mode is torque
        When the CAN SelectControlMode command is received with mode speed
        Then the active control mode shall be speed
        And a SelectControlModeResponse shall be emitted

    # REQ-INT-CM-002 ---------------------------------------------------------------
    Scenario: SelectControlMode is rejected while motor is enabled
        Given the active control mode is speed
        And the CAN Start command is received
        When the CAN SelectControlMode command is received with mode torque
        Then the active control mode shall be speed
        And a SelectControlModeResponse shall be emitted with reason busy

    # REQ-INT-CM-003 ---------------------------------------------------------------
    Scenario: Selected control mode is persisted across power cycles
        Given the active control mode is position
        When the system is restarted
        Then the active control mode shall be position

    # REQ-INT-CM-004 ---------------------------------------------------------------
    Scenario Outline: Typed setpoint rejected when mode does not match
        Given the active control mode is <active_mode>
        When the CAN <command> command is received with value 100
        Then a CommandRejected frame shall be emitted with reason controlModeMismatch

        Examples:
            | active_mode | command             |
            | torque      | SetSpeedSetpoint    |
            | torque      | SetPositionSetpoint |
            | speed       | SetTorqueSetpoint   |
            | position    | SetTorqueSetpoint   |

    # REQ-INT-CM-005 ---------------------------------------------------------------
    Scenario: Typed setpoint reaches active controller when mode matches
        Given the active control mode is speed
        When the CAN SetSpeedSetpoint command is received with value 300
        Then no CommandRejected frame shall be emitted
