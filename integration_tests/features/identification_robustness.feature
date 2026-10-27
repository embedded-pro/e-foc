Feature: Identification Against a Plant That Behaves Like Real Hardware
  The reference motors are ideal: continuous encoder, viscous friction only, a supply that
  never sags. The bench motor is not. Its winding, rotor, supply and 4000-count encoder come
  from the identified values of the real test rig, and it adds the dry friction, the encoder
  quantisation and the supply impedance real hardware has. Nothing here is a datasheet value
  except the power stage's switches and diodes and the current-sense chain's converter and
  amplifiers; see Parts K, L and M of documentation/design/software-in-the-loop.md.

  The limits are pinned from measured runs with a margin.

  The scenarios here take the bench's power stage and current-sense chain end to
  end through the emulated target, and the current-limited supply relies on the
  emulated board's undervoltage protection. The sweeps over dry friction, a
  once-per-turn torque, dead time with and without compensation, encoder
  quantisation and encoder noise run in the host model-in-the-loop tests
  (integration_tests/model_in_the_loop) with the same limits.

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
