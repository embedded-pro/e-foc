Feature: Motor Plant Characteristics
  The plant the target runs against is described per scenario: winding and rotor
  parameters, ADC and encoder noise, the thermal model and a load torque. The
  control loops must keep the motor turning across those conditions, and the
  same seed must give the same run twice.

  @sil @REQ-SM-006
  Scenario Outline: Speed control holds up against a <label> plant
    Given a <preset> motor plant
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 20 rad/s is applied
    Then the state machine shall be in the running state
    And the rotor shall turn

    Examples:
      | label            | preset  |
      | noisy            | noisy   |
      | hot              | hot     |
      | mechanically load| loaded  |

  @sil @REQ-SM-006
  Scenario: A motor wound with different parameters still runs
    Given a motor plant with:
      | stator_resistance_ohm        | 0.2      |
      | d_axis_inductance_henry      | 0.0012   |
      | q_axis_inductance_henry      | 0.0012   |
      | flux_linkage_weber           | 0.012    |
      | pole_pairs                   | 7        |
      | rotor_inertia_kg_m2          | 0.00002  |
      | viscous_damping_nm_s_per_rad | 0.00005  |
      | supply_voltage_volts         | 24.0     |
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a speed setpoint of 20 rad/s is applied
    Then the state machine shall be in the running state
    And the rotor shall turn

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
