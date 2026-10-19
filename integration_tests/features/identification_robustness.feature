Feature: Identification Against a Plant That Behaves Like Real Hardware
  The reference motors are ideal: continuous encoder, viscous friction only, a supply that
  never sags. The bench motor is not. Its winding, rotor, supply and 4000-count encoder come
  from the identified values of the real test rig, and it adds the dry friction, the encoder
  quantisation and the supply impedance real hardware has. Nothing here is a datasheet value;
  see Part K of documentation/design/software-in-the-loop.md.

  The limits are pinned from measured runs with a margin. Two targets the firmware does not
  meet yet, an unbiased inertia on a quantised encoder and a calibration of the light rotor
  under encoder noise, are listed in documentation/design/hardware-characterisation.md and
  join this file with the fix, not before.

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
    And the full calibration is run from the terminal
    Then the identified resistance shall be within 5 % of the plant
    And the identified pole pairs shall match the plant
    And the identified inertia shall be within 20 % of the plant

    Examples:
      | coulomb |
      | 0.0     |
      | 0.0005  |
      | 0.002   |

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
    And the full calibration is run from the terminal
    Then the motor shall not report a fault
    And the identified inertia shall be within 20 % of the plant

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
    And the full calibration is run from the terminal
    Then the identified inertia shall be within 20 % of the plant

    Examples:
      | sigma   |
      | 0.00005 |
      | 0.0002  |
