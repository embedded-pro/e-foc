Feature: Identification Against a Plant That Behaves Like Real Hardware
  The reference motors are ideal: continuous encoder, viscous friction only, a supply that
  never sags. The bench motor is not. Its winding, rotor, supply and 4000-count encoder come
  from the identified values of the real test rig, and it adds the dry friction, the encoder
  quantisation and the supply impedance real hardware has. Nothing here is a datasheet value
  except the power stage's switches and diodes and the current-sense chain's converter and
  amplifiers; see Parts K, L and M of documentation/design/software-in-the-loop.md.

  The limits are pinned from measured runs with a margin.

  @sil @REQ-CAL-013
  Scenario Outline: The full calibration identifies the bench motor with dry friction (<coulomb> Nm)
    Given a bench motor plant
    And a motor plant with:
      | coulomb_friction_nm | <coulomb> |
    And the motor is already calibrated
    And the stored calibration is off by:
      | inertia  | 2.0 |
      | friction | 0.5 |
    And the motor boots in speed mode
    When the target boots
    And the full calibration is run
    Then the identified resistance shall be within 5 % of the plant
    And the identified pole pairs shall match the plant
    And the identified inertia shall be within 20 % of the plant

    Examples:
      | coulomb |
      | 0.0     |
      | 0.0005  |
      | 0.002   |

  @sil @REQ-CAL-013
  Scenario: The full calibration reads the viscous friction through a once-per-turn torque
    Given a bench motor plant
    And a motor plant with:
      | viscous_damping_nm_s_per_rad  | 0.000012 |
      | coulomb_friction_nm           | 0.0037   |
      | cogging_torque_nm             | 0.001    |
      | cogging_cycles_per_revolution | 1        |
    And the motor is already calibrated
    And the stored calibration is off by:
      | inertia  | 2.0 |
      | friction | 0.5 |
    And the motor boots in speed mode
    When the target boots
    And the full calibration is run
    Then the identified friction shall be within 10 % of the plant
    And the identified inertia shall be within 5 % of the plant

  @sil @REQ-CAL-012
  Scenario Outline: The electrical identification reads through a non-ideal power stage (<dead> ns dead time)
    Given a bench motor plant
    And a motor plant with:
      | dead_time_ns               | <dead> |
      | switch_node_capacitance_pf | 300    |
      | switch_on_resistance_ohm   | 0.0125 |
      | diode_forward_volts        | 0.8    |
    And the motor boots in torque mode
    When the target boots
    And electrical identification is run
    Then the identified resistance shall be within <resistance> % of the plant
    And the identified pole pairs shall match the plant

    Examples:
      | dead | resistance |
      | 0    | 5          |
      | 50   | 5          |
      | 270  | 15         |

  @sil @REQ-CAL-012
  Scenario: Uncompensated, the bench's dead time reads the calibration resistance high
    Given a bench motor plant
    And the plant drives its windings through the bench's power stage
    And the motor boots in torque mode
    When the target boots
    And electrical identification is run
    Then the identified resistance shall read at least 3.5 % above the plant

  @sil @REQ-CAL-012
  Scenario: Compensating the bench's dead time keeps the calibration resistance on the winding
    Given a bench motor plant
    And the plant drives its windings through the bench's power stage
    And the drive compensates the bench's dead time
    And the motor boots in torque mode
    When the target boots
    And electrical identification is run
    Then the identified resistance shall be within 3 % of the plant

  @sil @REQ-TRQ-008
  Scenario: Uncompensated, the bench's dead time reads the online resistance high
    Given a bench motor plant
    And the plant drives its windings through the bench's power stage
    And the plant response is recorded at 100 Hz for up to 2000 samples
    And the motor is already calibrated
    And the stored calibration is off by:
      | resistance | 1.3 |
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And the speed setpoint alternates between 26 and 52 rad/s every 250 ms for 6000 ms
    And the online estimates are read
    Then the online resistance estimate shall read at least 12 % above the plant

  @sil @REQ-TRQ-008
  Scenario: Compensating the bench's dead time keeps the online resistance on the winding
    Given a bench motor plant
    And the plant drives its windings through the bench's power stage
    And the drive compensates the bench's dead time
    And the plant response is recorded at 100 Hz for up to 2000 samples
    And the motor is already calibrated
    And the stored calibration is off by:
      | resistance | 1.3 |
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And the speed setpoint alternates between 26 and 52 rad/s every 250 ms for 6000 ms
    And the online estimates are read
    Then the online resistance estimate shall be within 5 % of the plant

  @sil @REQ-TRQ-008
  Scenario: Compensating the bench's dead time keeps the online resistance on the winding read through the current-sense chain
    Given a bench motor plant
    And the plant drives its windings through the bench's power stage
    And the plant reads its currents through the bench's current-sense chain
    And the drive compensates the bench's dead time
    And the plant response is recorded at 100 Hz for up to 2000 samples
    And the motor is already calibrated
    And the stored calibration is off by:
      | resistance | 1.3 |
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And the speed setpoint alternates between 26 and 52 rad/s every 250 ms for 6000 ms
    And the online estimates are read
    Then the online resistance estimate shall be within 5 % of the plant

  @sil @REQ-CAL-013 @REQ-PERF-004
  Scenario: The full calibration of the bench motor runs through its power stage and current-sense chain together
    Given a bench motor plant
    And the plant drives its windings through the bench's power stage
    And the plant reads its currents through the bench's current-sense chain
    And the drive compensates the bench's dead time
    And the motor is already calibrated
    And the stored calibration is off by:
      | inertia  | 2.0 |
      | friction | 0.5 |
    And the motor boots in speed mode
    When the target boots
    And the control loop statistics are cleared
    And the full calibration is run
    Then the motor shall not report a fault
    And the identified resistance shall be within 5 % of the plant
    And the identified pole pairs shall match the plant
    And the identified inertia shall be within 5 % of the plant
    And the control loop shall have stayed within its budget

  @sil @REQ-CAL-012
  Scenario: The electrical identification reads through the bench's current-sense chain
    Given a bench motor plant
    And the plant reads its currents through the bench's current-sense chain
    And the motor boots in torque mode
    When the target boots
    And electrical identification is run
    Then the identified resistance shall be within 5 % of the plant
    And the identified pole pairs shall match the plant

  @sil @REQ-CAL-013
  Scenario: The calibration of the bench motor fits inside a current-limited supply
    Given a bench motor plant
    And a motor plant with:
      | supply_current_limit_ampere | 1.0  |
      | under_voltage_trip_volts    | 12.0 |
    And the motor is already calibrated
    And the stored calibration is off by:
      | inertia  | 2.0 |
      | friction | 0.5 |
    And the motor boots in speed mode
    When the target boots
    And the full calibration is run
    Then the motor shall not report a fault
    And the identified inertia shall be within 20 % of the plant

  @sil @REQ-CAL-013
  Scenario Outline: The inertia is unbiased on a quantised encoder (<motor>, <counts> counts)
    Given a <motor> motor plant
    And a motor plant with:
      | encoder_counts_per_revolution | <counts> |
    And the motor is already calibrated
    And the stored calibration is off by:
      | inertia  | 2.0 |
      | friction | 0.5 |
    And the motor boots in speed mode
    When the target boots
    And the full calibration is run
    Then the identified inertia shall be within <limit> % of the plant

    Examples:
      | motor   | counts | limit |
      | bench   | 4000   | 1     |
      | bench   | 20000  | 1     |
      | bench   | 1000   | 1     |
      | teknic  | 4000   | 1     |
      | anaheim | 4000   | 1     |

  @sil @REQ-CAL-013
  Scenario Outline: The calibration of the bench motor completes with encoder noise (<sigma> rad)
    Given a bench motor plant
    And a motor plant with:
      | encoder_noise_sigma_radians | <sigma> |
    And the motor is already calibrated
    And the stored calibration is off by:
      | inertia  | 2.0 |
      | friction | 0.5 |
    And the motor boots in speed mode
    When the target boots
    And the full calibration is run
    Then the identified inertia shall be within 20 % of the plant

    Examples:
      | sigma   |
      | 0.00005 |
      | 0.0002  |

  @sil @REQ-CAL-013
  Scenario Outline: The calibration of the <motor> motor completes with encoder noise (<sigma> rad)
    Given a <motor> motor plant
    And a motor plant with:
      | encoder_noise_sigma_radians | <sigma> |
    And the motor is already calibrated
    And the stored calibration is off by:
      | inertia  | 2.0 |
      | friction | 0.5 |
    And the motor boots in speed mode
    When the target boots
    And the full calibration is run
    Then the identified inertia shall be within 10 % of the plant

    Examples:
      | motor   | sigma  |
      | teknic  | 0.0002 |
      | anaheim | 0.0002 |
