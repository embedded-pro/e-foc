Feature: Board Protection Faults
  The simulated board protection stands in for the ADC comparators a real board
  wires to the PWM fault inputs. Like those comparators it is armed with the
  inverter, so a trip is raised against a switching drive, reaches the state
  machine, and is reported over CAN telemetry.

  The trip scenarios are tagged @sil-protection and held out of the default run.
  They reproduce a firmware lockup: delivering a board protection fault to a
  running drive takes an unaligned-access HardFault inside the fault path, which
  faults again and escalates. See documentation/design/software-in-the-loop.md.

  @sil-protection @REQ-SM-008
  Scenario Outline: A <fault> trip faults the running motor
    Given a nominal motor plant
    And a motor plant with:
      | <threshold>          | <value> |
      | supply_voltage_scale | <scale> |
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the rotor is aligned
    And the motor start command is issued
    Then the state machine shall report an <fault> fault
    And the motor shall refuse to run

    Examples:
      | fault           | threshold                     | value | scale |
      | overvoltage     | over_voltage_trip_volts       | 40.0  | 1.0   |
      | undervoltage    | under_voltage_trip_volts      | 40.0  | 0.5   |
      | overtemperature | over_temperature_trip_celsius | 90.0  | 1.0   |
      | overcurrent     | over_current_trip_ampere      | 0.5   | 1.0   |

  @sil @REQ-SM-008
  Scenario: A healthy plant within its protection limits keeps running
    Given a nominal motor plant
    And a motor plant with:
      | over_voltage_trip_volts       | 60.0  |
      | under_voltage_trip_volts      | 20.0  |
      | over_temperature_trip_celsius | 150.0 |
      | over_current_trip_ampere      | 150.0 |
    And the motor is already calibrated
    And the motor boots in torque mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And a torque setpoint of 0.5 A is applied
    Then the state machine shall be in the running state
