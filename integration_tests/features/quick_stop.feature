Feature: Quick Stop
  An emergency stop cuts the power stage and leaves the rotor to coast. A quick stop decelerates
  it under control instead (REQ-LCY-006): the power stage stays on, speed and position mode ramp
  the speed reference from the measured speed to zero at the quick-stop deceleration, and torque
  mode brakes with a current against the speed, capped at the one that decelerates the calibrated
  inertia at that rate. Once the speed is below 2 rad/s, or once a quarter more than the time the
  deceleration needs from the initial speed has passed, the drive cuts the power stage and
  reports Ready; meanwhile it reports Stopping.

  From 100 rad/s at 500 rad/s2 the reference reaches 2 rad/s after 196 ms; the limits are
  that time +/- 10 %. The torque-mode rotor turns at the overspeed limit of 100 rad/s, held by
  the speed limiter, when the quick stop is issued.

  @sil @REQ-LCY-006
  Scenario: A quick stop at 500 rad/s2 brings a speed-mode rotor from 100 rad/s below 2 rad/s in 196 ms
    Given a nominal motor plant
    And the plant response is recorded at 1000 Hz for up to 2000 samples
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the quickStopDeceleration is set to 500 over CAN
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 100 rad/s is applied
    And the response is captured for 1000 ms after enable
    And a quick stop is issued over CAN
    Then the state machine shall report the stopping state
    And the rotor speed shall fall below 2 rad/s between 176 and 216 ms after the quick stop
    And the state machine shall be in the ready state within 1000 ms

  @sil @REQ-LCY-006
  Scenario: A quick stop brakes a torque-mode rotor against its speed
    Given a nominal motor plant
    And the plant response is recorded at 1000 Hz for up to 2000 samples
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the quickStopDeceleration is set to 500 over CAN
    And the overspeedLimit is set to 100 over CAN
    And the rotor is aligned
    And the motor is enabled
    And a torque setpoint of 1 A is applied
    And the response is captured for 1000 ms after enable
    And a quick stop is issued over CAN
    Then the rotor speed shall fall below 2 rad/s between 150 and 230 ms after the quick stop
    And the state machine shall be in the ready state within 1000 ms

  @sil @REQ-LCY-006
  Scenario: The quick-stop deceleration is stored and read back after a restart
    Given a nominal motor plant
    When the target boots
    And the quickStopDeceleration is set to 750 over CAN
    And the target restarts
    Then the quickStopDeceleration read over CAN shall be 750

  @hil_motor @REQ-LCY-006
  Scenario: A quick stop at 500 rad/s2 brings the board's speed-mode rotor from 50 rad/s below 2 rad/s
    On the board the speed-mode rotor turns at 50 rad/s, within the bench's 60 rad/s, and the speed is
    polled over CAN from the quick stop's acknowledgement: the reference reaches 2 rad/s after 96 ms,
    and the limits are pinned from the bench's [METRIC] lines with a margin.

    Given the speed motor system is initialised with no valid calibration data
    When the speed control mode is selected
    And the full calibration is run
    And the quickStopDeceleration is set to 500 over CAN
    And the motor is enabled
    And a speed setpoint of 50 rad/s is applied
    Then the measured speed shall settle at 50 rad/s within 4 rad/s in 3000 ms
    When a quick stop is issued over CAN
    Then the measured speed shall fall below 2 rad/s between 90 and 135 ms after the quick stop
    And the state machine shall be in the ready state within 1000 ms

  @hil_motor @REQ-LCY-006
  Scenario: The board stores the quick-stop deceleration and reads it back after a restart
    When the quickStopDeceleration is set to 750 over CAN
    And the target restarts
    Then the quickStopDeceleration read over CAN shall be 750
