Feature: EEPROM Read-Write Integrity
  The hardware target writes and reads calibration data from EEPROM.

  # REQ-HIL-004
  Scenario: EEPROM round-trip preserves calibration data byte-for-byte
    Given the hardware target is connected and responding
    When a calibration data block is written to EEPROM
    Then the data read back from EEPROM shall be identical to the written data
