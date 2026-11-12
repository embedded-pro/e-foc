Feature: Fault History
  A drive keeps the last eight entries into Fault across power cycles (REQ-PRO-006), so an
  intermittent fault can still be diagnosed after a restart. Each record holds the fault code,
  the power-up it happened in, the time since that power-up, the control mode, the filtered bus
  voltage and the peak torque current of the last 10 ms. A record is read over CAN by its age,
  0 the newest, and the history is emptied by an explicit command (REQ-COM-001). A store starts
  at most once a second, so each scenario leaves the drive two seconds before it restarts it.

  The nominal plant runs from a 40 V bus, so a 35 V maximum faults a starting drive with
  overvoltage and a 45 V minimum with undervoltage.

  @sil @REQ-PRO-006 @REQ-COM-001
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

  @sil @REQ-PRO-006 @REQ-COM-001
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

  @hil_motor @REQ-PRO-006 @REQ-COM-001
  Scenario: The board reads back the faults of two power-ups newest first after a restart
    The bench supply is about 24 V, so on the board a 20 V maximum faults a starting drive with
    overvoltage and a 30 V minimum with undervoltage; the start is at 0 A. The board counts its
    power-ups from its first boot, so the records are compared with each other, not with a number.

    Given the torque motor system is initialised with no valid calibration data
    When the torque control mode is selected
    And electrical identification is run
    And the fault history is cleared over CAN
    And the busVoltageMaximum is set to 20 over CAN
    And a torque setpoint of 0 A is applied
    And the motor start command is issued
    Then the state machine shall report an overvoltage fault
    When the drive is left for 2000 ms
    And the target restarts
    And the busVoltageMaximum is set to 0 over CAN
    And the torque control mode is selected
    And electrical identification is run
    And the busVoltageMinimum is set to 30 over CAN
    And a torque setpoint of 0 A is applied
    And the motor start command is issued
    Then the state machine shall report an undervoltage fault
    When the drive is left for 2000 ms
    And the target restarts
    Then fault record 0 shall be an undervoltage fault in torque mode
    And fault record 0 shall carry a bus voltage within 1.0 V of 24.3 V
    And fault record 1 shall be an overvoltage fault in torque mode
    And fault record 1 shall come from the power-up before that of record 0
    And the fault history shall hold no record 2

  @hil_motor @REQ-PRO-006 @REQ-COM-001
  Scenario: The board reports a fault at once, and its cleared history stays empty after a restart
    Given the torque motor system is initialised with no valid calibration data
    When the torque control mode is selected
    And electrical identification is run
    And the busVoltageMaximum is set to 20 over CAN
    And a torque setpoint of 0 A is applied
    And the motor start command is issued
    Then the state machine shall report an overvoltage fault
    And fault record 0 shall be an overvoltage fault in torque mode
    When the fault history is cleared over CAN
    And the target restarts
    Then the fault history shall hold no record 0
