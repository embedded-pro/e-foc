Feature: Drive Limits
  The overspeed limit is one drive-wide setting of the configuration record (REQ-PRT-003). In
  torque mode nothing else bounds the speed: the torque cascade folds a current that pushes the
  rotor towards the limit to zero over its last tenth, and leaves a braking current alone. In
  speed mode a setpoint beyond the limit is held at it. In every mode a rotor that an outside
  torque drives past 110 % of the limit faults the drive with overspeed.

  In position mode a position error that stays beyond the following-error window for the
  following-error time faults the drive (REQ-PRT-004). The window is off by default, since a
  position step is itself an error as large as the step. The hand that grips the shaft makes
  the error; stall reporting is turned off so that only the following error can answer it.

  In speed mode the acceleration and deceleration bound how fast the speed reference may move
  towards the setpoint (REQ-SPD-010), so a step of the setpoint becomes a ramp of the speed.

  @sil @REQ-PRT-003
  Scenario: A torque command without load settles below the overspeed limit
    Given a nominal motor plant
    And the plant response is recorded at 100 Hz for up to 300 samples
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the overspeedLimit is set to 100 over CAN
    And the rotor is aligned
    And the motor is enabled
    And a torque setpoint of 1 A is applied
    And the response is captured for 2000 ms after enable
    Then the state machine shall be in the running state
    And the peak speed shall stay below 102 rad/s
    And the reported speed shall settle within 6 rad/s of 95 rad/s

  @sil @REQ-PRT-003
  Scenario: An outside torque that drives the rotor past the limit faults the drive
    Given a nominal motor plant
    And a torque step of -0.02 Nm applied 1000 ms after enable
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the overspeedLimit is set to 50 over CAN
    And the rotor is aligned
    And the motor is enabled
    And a torque setpoint of 0.5 A is applied
    Then the state machine shall report an overspeed fault

  @sil @REQ-PRT-003
  Scenario: A speed setpoint beyond the overspeed limit is held at the limit
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the overspeedLimit is set to 50 over CAN
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 80 rad/s is applied
    Then the reported speed shall settle within 2 rad/s of 50 rad/s
    And the state machine shall be in the running state

  @sil @REQ-PRT-004
  Scenario: A shaft held beyond the following-error window faults the drive
    Given a nominal motor plant
    And a hand turns the shaft by 0.5 rad over 100 ms, 1000 ms after enable, and holds it
    And the motor is already calibrated
    And the motor boots in position mode
    When the target boots
    And the stallDetection is set to 0 over CAN
    And the followingErrorWindow is set to 0.3 over CAN
    And the followingErrorTime is set to 0.1 over CAN
    And the rotor is aligned
    And a position setpoint of 0.0 rad is applied
    And the motor is enabled
    Then the state machine shall report a following-error fault

  @sil @REQ-PRT-004
  Scenario: A position step inside the following-error window leaves the drive running
    Given a nominal motor plant
    And the plant response is recorded at 100 Hz for up to 300 samples
    And the motor is already calibrated
    And the motor boots in position mode
    When the target boots
    And the followingErrorWindow is set to 2.0 over CAN
    And the followingErrorTime is set to 0.05 over CAN
    And the rotor is aligned
    And a position setpoint of 0.0 rad is applied
    And the motor is enabled
    And a position setpoint of 1.5 rad is applied
    And the response is captured for 1500 ms after enable
    Then the state machine shall be in the running state

  @sil @REQ-SPD-010
  Scenario: A speed step follows the acceleration ramp
    Given a nominal motor plant
    And the plant response is recorded at 1000 Hz for up to 800 samples
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the speedAcceleration is set to 100 over CAN
    And the rotor is aligned
    And a speed setpoint of 20 rad/s is applied
    And the motor is enabled
    And the response is captured for 600 ms after enable
    Then the speed step response shall take at least 140 ms to rise
    And the speed step response shall settle into a 5 % band within 230 ms with overshoot below 3.5 %

  @sil @REQ-PRT-003 @REQ-PRT-004 @REQ-SPD-010 @REQ-CAN-015
  Scenario: The limits are stored and read back after a restart
    Given a nominal motor plant
    When the target boots
    And the overspeedLimit is set to 250 over CAN
    And the followingErrorWindow is set to 0.4 over CAN
    And the followingErrorTime is set to 0.25 over CAN
    And the speedAcceleration is set to 300 over CAN
    And the speedDeceleration is set to 600 over CAN
    And the target restarts
    Then the overspeedLimit read over CAN shall be 250
    And the followingErrorWindow read over CAN shall be 0.4
    And the followingErrorTime read over CAN shall be 0.25
    And the speedAcceleration read over CAN shall be 300
    And the speedDeceleration read over CAN shall be 600
