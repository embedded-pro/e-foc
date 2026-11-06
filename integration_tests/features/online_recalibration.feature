Feature: Online Recalibration
  In speed and position mode the drive re-estimates its motor while it runs. Online
  recalibration lets the loops follow a settled estimate, but only while the drive is stopped:
  on every stop of a running drive in automatic mode, or on a Recalibrate command in the
  on-command mode (REQ-CAL-023, REQ-CAL-024). The calibration record keeps the commissioned
  values; the model the loops use is read over CAN (REQ-CAN-024). The mode is off by default.
  A recalibrated inductance, inertia and friction are stored, at most once every 15 minutes, and
  in use again after a restart until a Reset Model command (REQ-CAL-025, REQ-CAN-025).

  Each scenario stores an inertia 1.5 times the plant's, which the online estimator corrects
  while the speed steps between two levels.

  @sil @REQ-CAL-023 @REQ-CAL-024
  Scenario: An automatic recalibration adopts the settled inertia at the stop
    Given a nominal motor plant
    And the plant response is recorded at 100 Hz for up to 2000 samples
    And the motor is already calibrated
    And the stored calibration is off by:
      | inertia | 1.5 |
    And the motor boots in speed mode
    When the target boots
    And the recalibrationMode is set to 2 over CAN
    And the rotor is aligned
    And the motor is enabled
    And the speed setpoint alternates between 26 and 52 rad/s every 250 ms for 12000 ms
    And the motor is disabled
    And the model in use is read
    Then the inertia in use shall be within 10 % of the plant

  @sil @REQ-CAL-024 @REQ-CAN-024
  Scenario: On command, the settled inertia is adopted once the drive is stopped and asked
    Given a nominal motor plant
    And the plant response is recorded at 100 Hz for up to 2000 samples
    And the motor is already calibrated
    And the stored calibration is off by:
      | inertia | 1.5 |
    And the motor boots in speed mode
    When the target boots
    And the recalibrationMode is set to 1 over CAN
    And the rotor is aligned
    And the motor is enabled
    And the speed setpoint alternates between 26 and 52 rad/s every 250 ms for 6000 ms
    And the motor is disabled
    Then the recalibrationsApplied read over CAN shall be 0
    When the drive is recalibrated over CAN
    And the model in use is read
    Then the inertia in use shall be within 10 % of the plant

  @sil @REQ-CAN-024
  Scenario: A recalibration is refused while the mode is off
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    Then a recalibration over CAN shall be refused

  @sil @REQ-CAN-024
  Scenario: The recalibration mode is stored and read back after a restart
    Given a nominal motor plant
    When the target boots
    And the recalibrationMode is set to 2 over CAN
    And the target restarts
    Then the recalibrationMode read over CAN shall be 2

  @sil @REQ-CAL-025 @REQ-CAN-025
  Scenario: An automatic recalibration is stored once and in use after a restart
    Given a nominal motor plant
    And the plant response is recorded at 100 Hz for up to 2000 samples
    And the motor is already calibrated
    And the stored calibration is off by:
      | inertia | 1.5 |
    And the motor boots in speed mode
    When the target boots
    And the recalibrationMode is set to 2 over CAN
    And the rotor is aligned
    And the motor is enabled
    And the speed setpoint alternates between 26 and 52 rad/s every 250 ms for 12000 ms
    And the motor is disabled
    And the motor is enabled
    And the speed setpoint alternates between 26 and 52 rad/s every 250 ms for 6000 ms
    And the motor is disabled
    Then the recalibrationsStored read over CAN shall be 1
    When the target restarts
    And the rotor is aligned
    And the model in use is read
    Then the inertia in use shall be within 10 % of the plant

  @sil @REQ-CAL-025 @REQ-CAN-025
  Scenario: Reset Model returns a restarted drive to the calibration record
    Given a nominal motor plant
    And the plant response is recorded at 100 Hz for up to 2000 samples
    And the motor is already calibrated
    And the stored calibration is off by:
      | inertia | 1.5 |
    And the motor boots in speed mode
    When the target boots
    And the recalibrationMode is set to 2 over CAN
    And the rotor is aligned
    And the motor is enabled
    And the speed setpoint alternates between 26 and 52 rad/s every 250 ms for 12000 ms
    And the motor is disabled
    And the model is reset over CAN
    Then the recalibrationsStored read over CAN shall be 2
    When the target restarts
    And the rotor is aligned
    And the model in use is read
    Then the inertia in use shall be within 0.01 % of the stored calibration
