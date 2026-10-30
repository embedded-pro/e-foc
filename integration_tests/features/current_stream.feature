Feature: Streamed Currents and Rotor Position
  While a CAN client is online and a closed-loop mode runs, the drive sends one
  TelemetryCurrents frame per window of the configured telemetry rate: the window
  means of the measured d- and q-axis currents and the rotor position at the
  window's end, numbered so that a window the drive could not send shows as a gap.
  The emulator shares its host with other work, so a run may lose a window to a host
  stall longer than the stream's sixteen-window buffer; one gap in a second is allowed.
  The torque run holds the rotor with dry friction above the commanded torque: a free
  rotor would accelerate into the voltage limit, where the current can no longer follow
  its setpoint, so the stream's mean is checked against a current the loop can hold.

  @sil @REQ-CAN-012
  Scenario: A torque run streams contiguous one-millisecond windows of the measured currents
    Given a nominal motor plant
    And a motor plant with:
      | coulomb_friction_nm | 0.05 |
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the rotor is aligned
    And the telemetry rate is set to 1000 Hz
    And the motor is enabled
    And a torque setpoint of 0.5 A is applied
    And 1000 streamed current samples are collected
    Then the streamed sequence shall have at most 1 gaps
    And the mean streamed q-axis current shall be within 0.002 A of 0.5 A
    And the mean streamed d-axis current shall be within 0.002 A of 0.0 A
    And every streamed position shall lie within half a turn of the alignment zero

  @sil @REQ-CAN-012
  Scenario: A zero telemetry rate stops the stream
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the rotor is aligned
    And the telemetry rate is set to 0 Hz
    And the motor is enabled
    And a torque setpoint of 0.5 A is applied
    Then no streamed current sample shall arrive within 500 ms
