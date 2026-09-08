Feature: CAN Control Mode Selection and Typed Setpoints
    # Covers: REQ-CM-001 .. REQ-CM-005

    Background:
        Given the multi-mode system is initialised with valid calibration data
        And the CAN category server is connected to the control mode coordinator

    # REQ-CM-001 ---------------------------------------------------------------
        @sil
    Scenario: SelectControlMode switches the active controller
        Given the active control mode is torque
        When the CAN SelectControlMode command is received with mode speed
        Then the active control mode shall be speed
        And a SelectControlModeResponse shall be emitted

    # REQ-CM-002 ---------------------------------------------------------------
        @sil
    Scenario: SelectControlMode is rejected while motor is enabled
        Given the active control mode is speed
        And the CAN Start command is received
        When the CAN SelectControlMode command is received with mode torque
        Then the active control mode shall be speed
        And a SelectControlModeResponse shall be emitted with reason busy

    # REQ-CM-003 ---------------------------------------------------------------
        @sil
    Scenario: Selected control mode is persisted across power cycles
        Given the active control mode is position
        When the system is restarted
        Then the active control mode shall be position

    # REQ-CM-004 ---------------------------------------------------------------
        @sil
    Scenario Outline: Typed setpoint rejected when mode does not match
        Given the active control mode is <active_mode>
        When the CAN <command> command is received with value 100
        Then a CommandRejected frame shall be emitted with reason controlModeMismatch

                @sil
        Examples:
            | active_mode | command             |
            | torque      | SetSpeedSetpoint    |
            | torque      | SetPositionSetpoint |
            | speed       | SetTorqueSetpoint   |
            | position    | SetTorqueSetpoint   |

    # REQ-CM-005 ---------------------------------------------------------------
        @sil
    Scenario: Typed setpoint reaches active controller when mode matches
        Given the active control mode is speed
        When the CAN SetSpeedSetpoint command is received with value 300
        Then no CommandRejected frame shall be emitted
