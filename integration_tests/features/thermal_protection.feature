Feature: Thermal Protection
  The board has no temperature input, so speed and position mode infer the winding's temperature
  from the online resistance estimate (REQ-PRT-002): copper's resistance rises 0.393 % per kelvin
  above the resistance the calibration measured at the calibration temperature. The estimator's
  excitation bursts run 4 s at the start of every 60 s, so the estimate has moved from the
  calibration temperature to the winding's by the end of the first burst after an Enable. Torque
  mode runs no estimator and reports the temperature unknown.

  @sil @REQ-PRT-002 @REQ-CAN-019
  Scenario: In speed mode the winding temperature follows a winding warmer than at the calibration
    Given a nominal motor plant
    And a motor plant with:
      | ambient_celsius | 60.0 |
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    Then the windingTemperature read over CAN shall come within 0.5 of 25 within 1000 ms
    When the motor is enabled
    And a speed setpoint of 30 rad/s is applied
    Then the windingTemperature read over CAN shall come within 1 of 60 within 8000 ms

  @sil @REQ-PRT-002
  Scenario: Torque mode reports the winding temperature unknown
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a torque setpoint of 0 A is applied
    Then the windingTemperature read over CAN shall be unknown

  @sil @REQ-CAN-019
  Scenario: The calibration temperature is stored and read back after a restart
    Given a nominal motor plant
    When the target boots
    And the calibrationTemperature is set to 40 over CAN
    And the target restarts
    Then the calibrationTemperature read over CAN shall be 40
