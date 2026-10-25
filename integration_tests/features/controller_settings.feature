Feature: Controllers and Settings Over CAN
  From contract 2.1 a CAN client chooses the controller of each loop the active control mode
  runs, writes the tuning settings and the motor parameters, and reads back what the drive holds.
  What it writes is stored, so it is still in force after the drive restarts.

  @sil @REQ-CTRL-013 @REQ-CAN-007
  Scenario: A controller chosen over CAN is reported and runs
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the speed loop controller is set to adrc over CAN
    Then the speed loop controller read over CAN shall be adrc
    When the rotor is aligned
    Then the speed loop shall be running the adrc algorithm

  @sil @REQ-CTRL-015 @REQ-CAN-007 @REQ-CAN-008
  Scenario: A bandwidth set over CAN holds across a restart until it is set back to automatic
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    Then the speedBandwidth read over CAN shall be automatic
    When the speedBandwidth is set to 120 over CAN
    And the target restarts
    Then the speedBandwidth read over CAN shall be 120
    When the speedBandwidth is set to automatic over CAN
    Then the speedBandwidth read over CAN shall be automatic

  @sil @REQ-CTRL-016
  Scenario: A switching gain at the boundary layer is refused over CAN and changes nothing
    Given a nominal motor plant
    And the motor is already calibrated
    When the target boots
    Then setting the currentSwitchingGain to 0.5 over CAN shall be refused as a design that cannot be made
    And the currentSwitchingGain read over CAN shall be 0.2

  @sil @REQ-SM-031 @REQ-CAN-007
  Scenario: A motor parameter set over CAN is stored and read back after a restart
    Given a nominal motor plant
    And the motor is already calibrated
    When the target boots
    And the inertia is set to 0.00002 over CAN
    And the target restarts
    Then the inertia read over CAN shall be 0.00002
