Feature: Bus-Voltage Limits
  The board's comparators trip at the bus voltages the power stage survives. An operator whose
  motor or supply is rated lower sets a bus-voltage minimum and maximum inside them
  (REQ-PRT-009): a running drive whose bus, read every 10 ms and filtered with a 50 ms time
  constant, leaves them faults with undervoltage or overvoltage, the codes a comparator trip
  reports. A stopped drive is not watched, so a supply switched off while the drive waits is no
  fault. Each limit is off at 0, its default.

  The nominal plant runs from a 40 V bus, so a 45 V minimum and a 35 V maximum each put it
  outside.

  @sil @REQ-PRT-009
  Scenario Outline: A bus outside the <limit> faults the motor that starts on it
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the rotor is aligned
    And the <limit> is set to <value> over CAN
    And a torque setpoint of 0.5 A is applied
    And the motor start command is issued
    Then the state machine shall report an <fault> fault

    Examples:
      | limit             | value | fault        |
      | busVoltageMinimum | 45.0  | undervoltage |
      | busVoltageMaximum | 35.0  | overvoltage  |

  @sil @REQ-PRT-009
  Scenario: A stopped motor on a bus below the minimum is not faulted
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the rotor is aligned
    And the busVoltageMinimum is set to 45 over CAN
    Then the busVoltageMinimum read over CAN shall stay 45 for 500 ms
    When the busVoltageMinimum is set to 0 over CAN
    And the motor is enabled
    And a torque setpoint of 0.5 A is applied
    Then the state machine shall be in the running state

  @sil @REQ-PRT-009
  Scenario: A supply that sags below the minimum faults the running motor
    Given a nominal motor plant
    And the supply steps to 0.6 of its voltage 1000 ms after enable
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the busVoltageMinimum is set to 30 over CAN
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 30 rad/s is applied
    Then the state machine shall report an undervoltage fault

  @sil @REQ-CAN-021
  Scenario: The bus-voltage limits are stored and read back after a restart
    Given a nominal motor plant
    When the target boots
    And the busVoltageMinimum is set to 18 over CAN
    And the busVoltageMaximum is set to 48 over CAN
    And the target restarts
    Then the busVoltageMinimum read over CAN shall be 18
    And the busVoltageMaximum read over CAN shall be 48
