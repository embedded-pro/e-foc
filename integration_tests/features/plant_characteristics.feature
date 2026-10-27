Feature: Motor Plant Characteristics
  The plant the target runs against is described per scenario: winding and rotor
  parameters, ADC and encoder noise, the thermal model and a load torque. Whether
  the loops keep their envelope on a noisy, hot, loaded or differently wound plant
  is measured in control_robustness.feature; these scenarios cover the conditions
  that decide whether the drive aligns and runs at all.

  @sil @REQ-SM-004
  Scenario: Encoder noise above the alignment settle threshold prevents alignment
    Given a motor plant with:
      | encoder_noise_sigma_radians | 0.01 |
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the rotor alignment is attempted
    Then the motor shall refuse to enable

  @sil @REQ-SM-005
  Scenario: Heavy current measurement noise does not stop torque control
    Given a motor plant with:
      | adc_noise_sigma_ampere      | 0.2    |
      | adc_bias_ampere_a           | 0.1    |
      | encoder_noise_sigma_radians | 0.0002 |
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a torque setpoint of 0.5 A is applied
    Then the state machine shall be in the running state
