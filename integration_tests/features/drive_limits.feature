Feature: Drive Limits
  The overspeed limit is one drive-wide setting of the configuration record (REQ-PRO-004). In
  torque mode nothing else bounds the speed: the torque cascade folds a current that pushes the
  rotor towards the limit to zero over its last tenth, and leaves a braking current alone. In
  speed mode a setpoint beyond the limit is held at it. In every mode a rotor that an outside
  torque drives past 110 % of the limit faults the drive with overspeed.

  In position mode a position error that stays beyond the following-error window for the
  following-error time faults the drive (REQ-PRO-004). The window is off by default, since a
  position step is itself an error as large as the step. The hand that grips the shaft makes
  the error; stall reporting is turned off so that only the following error can answer it.

  In speed mode the acceleration and deceleration bound how fast the speed reference may move
  towards the setpoint (REQ-CTL-002), so a step of the setpoint becomes a ramp of the speed.

  A running drive whose CAN client stops sending Start and setpoints for longer than the command
  timeout faults with commandTimeout (REQ-LCY-005), even while the client's heartbeat keeps it
  online. The timeout is off by default.

  With a continuous current rating set, a thermal model of the winding lets a cold motor carry its
  peak current for the peak time and then folds the current limit back to the continuous rating
  (REQ-PRO-001). The hand holds the rotor so that the commanded current flows the whole time.

  Torque mode adds the cogging map a speed or position calibration stored to the operator's torque
  (REQ-CTL-001). On the bench, 0.12 A is more than the shaft's dry friction but less than the dry
  friction and the cogging together: with the map it turns the rotor, without one it leaves the
  rotor in a detent, a 24th of a turn wide.

  @sil @REQ-PRO-004
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

  @sil @REQ-PRO-004
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

  @sil @REQ-PRO-004
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

  @sil @REQ-PRO-004
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

  @sil @REQ-PRO-004
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

  @sil @REQ-CTL-002
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

  @sil @REQ-LCY-005
  Scenario: A client that stops sending setpoints faults the running drive
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the commandTimeout is set to 0.5 over CAN
    And the rotor is aligned
    And the motor start command is issued
    And a torque setpoint of 0.2 A is applied
    Then the state machine shall report a command-timeout fault

  @sil @REQ-LCY-005
  Scenario: With the command timeout off, a drive without new setpoints keeps running
    Given a nominal motor plant
    And the plant response is recorded at 100 Hz for up to 300 samples
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a torque setpoint of 0.2 A is applied
    And the response is captured for 2000 ms after enable
    Then the state machine shall be in the running state

  @sil @REQ-PRO-001
  Scenario: A held rotor carries its peak current for the peak time and then its continuous rating
    Given a nominal motor plant
    And the plant response is recorded at 100 Hz for up to 400 samples
    And a hand turns the shaft by 0.01 rad over 1 ms, 0 ms after enable, and holds it
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the continuousCurrent is set to 2 over CAN
    And the peakCurrent is set to 3 over CAN
    And the peakCurrentTime is set to 2 over CAN
    And the rotor is aligned
    And a torque setpoint of 3 A is applied
    And the motor is enabled
    And the response is captured for 3000 ms after enable
    Then the q-axis current shall average between 2.85 and 3.15 A from 200 to 1700 ms after enable
    And the q-axis current shall average between 1.9 and 2.1 A from 2400 to 3000 ms after enable
    And the state machine shall be in the running state

  @sil @REQ-CTL-001
  Scenario: With the map a speed calibration stored, a small torque turns the rotor through the detents
    Given a bench motor plant
    And the plant's shaft has the bench's dry friction and cogging
    And the plant response is recorded at 100 Hz for up to 3000 samples
    And the motor boots in speed mode
    When the target boots
    And the full calibration is run
    And the torque control mode is selected
    And the rotor is aligned
    And the motor is enabled
    And a torque setpoint of 0.12 A is applied
    And the response is captured for 2000 ms after the last setpoint
    Then the rotor shall travel at least 6.3 rad after the last setpoint

  @sil @REQ-CTL-001
  Scenario: Without a cogging map, the same torque leaves the rotor in a detent
    Given a bench motor plant
    And the plant's shaft has the bench's dry friction and cogging
    And the plant response is recorded at 100 Hz for up to 400 samples
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a torque setpoint of 0.12 A is applied
    And the response is captured for 2000 ms after the last setpoint
    Then the rotor shall travel less than 0.26 rad after the last setpoint

  @sil @REQ-PRO-004 @REQ-CTL-002 @REQ-CTL-010 @REQ-LCY-005 @REQ-PRO-001 @REQ-COM-001
  Scenario: The limits are stored and read back after a restart
    Given a nominal motor plant
    When the target boots
    And the overspeedLimit is set to 250 over CAN
    And the followingErrorWindow is set to 0.4 over CAN
    And the followingErrorTime is set to 0.25 over CAN
    And the speedAcceleration is set to 300 over CAN
    And the speedDeceleration is set to 600 over CAN
    And the profileVelocity is set to 20 over CAN
    And the profileAcceleration is set to 400 over CAN
    And the profileDeceleration is set to 500 over CAN
    And the profileJerk is set to 9000 over CAN
    And the commandTimeout is set to 1.5 over CAN
    And the continuousCurrent is set to 2 over CAN
    And the peakCurrent is set to 3 over CAN
    And the peakCurrentTime is set to 2.5 over CAN
    And the target restarts
    Then the overspeedLimit read over CAN shall be 250
    And the followingErrorWindow read over CAN shall be 0.4
    And the followingErrorTime read over CAN shall be 0.25
    And the speedAcceleration read over CAN shall be 300
    And the speedDeceleration read over CAN shall be 600
    And the profileVelocity read over CAN shall be 20
    And the profileAcceleration read over CAN shall be 400
    And the profileDeceleration read over CAN shall be 500
    And the profileJerk read over CAN shall be 9000
    And the commandTimeout read over CAN shall be 1.5
    And the continuousCurrent read over CAN shall be 2
    And the peakCurrent read over CAN shall be 3
    And the peakCurrentTime read over CAN shall be 2.5

  @hil_motor @REQ-PRO-004 @REQ-CTL-002 @REQ-CTL-010 @REQ-LCY-005 @REQ-PRO-001 @REQ-COM-001
  Scenario: The board stores the limits and reads them back after a restart
    On the board the overspeed limit stored is 50 rad/s, within the bench's 60 rad/s, and the command
    timeout is checked on a speed-mode rotor at 30 rad/s: a torque setpoint held that long would spin
    the free rotor far past the bench limit.

    When the overspeedLimit is set to 50 over CAN
    And the followingErrorWindow is set to 0.4 over CAN
    And the followingErrorTime is set to 0.25 over CAN
    And the speedAcceleration is set to 300 over CAN
    And the speedDeceleration is set to 600 over CAN
    And the profileVelocity is set to 20 over CAN
    And the profileAcceleration is set to 400 over CAN
    And the profileDeceleration is set to 500 over CAN
    And the profileJerk is set to 9000 over CAN
    And the commandTimeout is set to 1.5 over CAN
    And the continuousCurrent is set to 2 over CAN
    And the peakCurrent is set to 3 over CAN
    And the peakCurrentTime is set to 2.5 over CAN
    And the target restarts
    Then the overspeedLimit read over CAN shall be 50
    And the followingErrorWindow read over CAN shall be 0.4
    And the followingErrorTime read over CAN shall be 0.25
    And the speedAcceleration read over CAN shall be 300
    And the speedDeceleration read over CAN shall be 600
    And the profileVelocity read over CAN shall be 20
    And the profileAcceleration read over CAN shall be 400
    And the profileDeceleration read over CAN shall be 500
    And the profileJerk read over CAN shall be 9000
    And the commandTimeout read over CAN shall be 1.5
    And the continuousCurrent read over CAN shall be 2
    And the peakCurrent read over CAN shall be 3
    And the peakCurrentTime read over CAN shall be 2.5

  @hil_motor @REQ-LCY-005
  Scenario: A client that stops sending setpoints faults the board's running drive
    Given the speed motor system is initialised with no valid calibration data
    When the speed control mode is selected
    And the full calibration is run
    And the commandTimeout is set to 2 over CAN
    And a speed setpoint of 30 rad/s is applied
    And the motor is enabled
    Then the measured speed shall settle at 30 rad/s within 4 rad/s in 1500 ms
    And the state machine shall report a command-timeout fault
