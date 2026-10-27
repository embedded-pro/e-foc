Feature: Non-Volatile Memory Integrity
  Stored calibration and configuration each carry a magic number, a layout
  version and a CRC. Damaged calibration must not be trusted, so the target
  falls back to an uncalibrated start rather than driving on bad parameters.
  Damaged configuration falls back to defaults, which are always usable. How each
  kind of damage (a wrong magic, a stale layout version, a bad CRC) is detected is
  covered by the NonVolatileMemoryImpl unit tests; here one damaged calibration
  record and one damaged configuration record check what the target does with them.

  @sil @REQ-SM-010
  Scenario Outline: A calibration record with a <damage> checksum is not trusted
    Given a nominal motor plant
    And the stored calibration has a <damage>
    When the target boots
    Then the state machine shall be in the Ready state
    And the motor shall refuse to enable

    Examples:
      | damage  |
      | corrupt |

  @sil @REQ-SM-010
  Scenario Outline: A configuration record with a <damage> checksum falls back to defaults
    Given a nominal motor plant
    And the motor is already calibrated
    And the stored configuration has a <damage>
    When the target boots
    And the rotor is aligned
    Then the current loop shall be running the pid algorithm
    And the state machine shall be in the Ready state

    Examples:
      | damage  |
      | corrupt |
