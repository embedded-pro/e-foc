Feature: Thermal Protection
  The board has no temperature input, so speed and position mode infer the winding's temperature
  from the online resistance estimate (REQ-PRO-002): copper's resistance rises 0.393 % per kelvin
  above the resistance the calibration measured at the calibration temperature. The estimator's
  excitation bursts run 4 s at the start of every 60 s, so the estimate has moved from the
  calibration temperature to the winding's by the end of the first burst after an Enable. Torque
  mode runs no estimator and reports the temperature unknown.

  While the temperature is known the current limit falls linearly over the temperature margin
  below the temperature limit, to zero at the limit, and at the limit the drive faults with
  overtemperature, not sooner than 5 s after the Enable (REQ-PRO-002). The protection acts on the
  estimate filtered with a 1 s time constant, so the current is measured once the filter has
  settled. In the derating scenario a
  strongly damped rotor settles well below a far speed setpoint, so the speed loop asks for all the
  current the limit allows, and a large thermal capacitance keeps the winding from heating
  noticeably while the current is measured.

  @sil @REQ-PRO-002 @REQ-COM-001
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

  @sil @REQ-PRO-002
  Scenario: Torque mode reports the winding temperature unknown
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a torque setpoint of 0 A is applied
    Then the windingTemperature read over CAN shall be unknown

  @sil @REQ-PRO-002
  Scenario: A winding near the temperature limit derates the current a damped rotor draws
    Given a nominal motor plant
    And a motor plant with:
      | ambient_celsius              | 110.0   |
      | thermal_capacitance          | 10000.0 |
      | viscous_damping_nm_s_per_rad | 0.003   |
    And the plant response is recorded at 100 Hz for up to 800 samples
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the temperatureLimit is set to 130 over CAN
    And the temperatureMargin is set to 40 over CAN
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 300 rad/s is applied
    And the response is captured for 7000 ms after enable
    Then the q-axis current shall average between 9.5 and 10.5 A from 6000 to 7000 ms after enable

  @sil @REQ-PRO-002
  Scenario: A winding past the temperature limit faults the drive
    Given a nominal motor plant
    And a motor plant with:
      | ambient_celsius | 90.0 |
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the stallDetection is set to 0 over CAN
    And the temperatureLimit is set to 80 over CAN
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 20 rad/s is applied
    Then the state machine shall report an overtemperature fault

  @sil @REQ-COM-001
  Scenario: The thermal settings are stored and read back after a restart
    Given a nominal motor plant
    When the target boots
    And the calibrationTemperature is set to 40 over CAN
    And the temperatureLimit is set to 100 over CAN
    And the temperatureMargin is set to 10 over CAN
    And the target restarts
    Then the calibrationTemperature read over CAN shall be 40
    And the temperatureLimit read over CAN shall be 100
    And the temperatureMargin read over CAN shall be 10
