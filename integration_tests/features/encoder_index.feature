Feature: Starting from the encoder index
  An incremental encoder loses its reference at every reset, so a drive without an index has to
  align the rotor again after each power-up. With an index channel the calibration measures the
  angle of the index in the frame the alignment set and stores it with the record.
  After a power-up the drive is Ready, and the first Start turns the rotor slowly with a bounded
  current until the index passes, sets the encoder so the index reads the stored angle, and runs
  without aligning (REQ-CMS-005).

  The plant's encoder reads 1 rad ahead of the rotor at power-up, a frame four electrical radians
  wrong on the bench motor: a drive that ran in it would push the rotor backwards. Its index sits
  2.5 rad from the electrical zero and is one count wide.

  @sil @REQ-CMS-005
  Scenario: A stored index angle restores the rotor reference at the first Start
    Given a bench motor plant
    And a motor plant with:
      | encoder_index_radians      | 2.5 |
      | encoder_index_width_counts | 1   |
      | encoder_bias_radians       | 1.0 |
    And the plant response is recorded at 1000 Hz for up to 2000 samples
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    Then the state machine shall be in the Ready state
    When the motor is started from the encoder index
    And a torque setpoint of 0.1 A is applied
    And the response is captured for 300 ms after enable
    Then the state machine shall be in the running state
    And the q-axis current shall average between 0.08 and 0.12 A from 100 to 300 ms after enable
    And the d-axis current shall be below 10 mA RMS from 100 to 300 ms after enable

  @sil @REQ-CMS-005
  Scenario: The calibration stores the index angle, and a restart starts from it
    Given a bench motor plant
    And a motor plant with:
      | encoder_index_radians      | 2.5 |
      | encoder_index_width_counts | 1   |
      | encoder_bias_radians       | 1.0 |
    And the motor boots in torque mode
    When the target boots
    And the calibrate command is issued
    Then the state machine shall be in the Ready state
    When the target restarts
    Then the state machine shall be in the Ready state
    When the motor is started from the encoder index
    And a torque setpoint of 0.1 A is applied
    Then the measured speed shall exceed 20 rad/s within 3000 ms
