Feature: Fault History
  A drive keeps the last eight entries into Fault across power cycles (REQ-PRT-011), so an
  intermittent fault can still be diagnosed after a restart. Each record holds the fault code,
  the power-up it happened in, the time since that power-up, the control mode, the filtered bus
  voltage and the peak torque current of the last 10 ms. A record is read over CAN by its age,
  0 the newest, and the history is emptied by an explicit command (REQ-CAN-023). A store starts
  at most once a second, so each scenario leaves the drive two seconds before it restarts it.

  The nominal plant runs from a 40 V bus, so a 35 V maximum faults a starting drive with
  overvoltage and a 45 V minimum with undervoltage.

  @sil @REQ-PRT-011 @REQ-CAN-023
  Scenario: The faults of two power-ups are read back newest first after a restart
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the rotor is aligned
    And the busVoltageMaximum is set to 35 over CAN
    And a torque setpoint of 0.5 A is applied
    And the motor start command is issued
    Then the state machine shall report an overvoltage fault
    When the drive is left for 2000 ms
    And the target restarts
    And the busVoltageMaximum is set to 0 over CAN
    And the busVoltageMinimum is set to 45 over CAN
    And the rotor is aligned
    And a torque setpoint of 0.5 A is applied
    And the motor start command is issued
    Then the state machine shall report an undervoltage fault
    When the drive is left for 2000 ms
    And the target restarts
    Then fault record 0 shall be an undervoltage fault in torque mode from power-up 2
    And fault record 0 shall carry a bus voltage within 1.0 V of 40.0 V
    And fault record 1 shall be an overvoltage fault in torque mode from power-up 1
    And the fault history shall hold no record 2

  @sil @REQ-PRT-011 @REQ-CAN-023
  Scenario: A fault is read at once, and a cleared history stays empty after a restart
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the rotor is aligned
    And the busVoltageMaximum is set to 35 over CAN
    And a torque setpoint of 0.5 A is applied
    And the motor start command is issued
    Then the state machine shall report an overvoltage fault
    And fault record 0 shall be an overvoltage fault in torque mode from power-up 1
    When the fault history is cleared over CAN
    And the target restarts
    Then the fault history shall hold no record 0
