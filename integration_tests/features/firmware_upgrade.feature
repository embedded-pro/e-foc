Feature: Firmware upgrade
  The application receives an upgrade pack over the can-lite firmware-upgrade
  category into the staging slot, checks it and keeps it staged; on Activate it
  marks the pack ready and restarts into the bootloader, which installs it. A
  download starts, and an apply restarts, only while the drive is stopped. Every
  pack is built with the release's pack builder and signed with the public
  development key set the emulated bootloader carries.

  Background:
    Given a nominal motor plant
    And the motor is already calibrated
    And the motor boots in torque mode

  @REQ-UPD-001 @REQ-UPD-004 @REQ-UPD-007 @REQ-UPD-013 @sil
  Scenario: A pack is downloaded and staged
    When the target boots
    And a pack of version "2.3.4" is downloaded
    Then Begin Upgrade was answered with a page size of 256
    And the download is answered "ok"
    And the journal reads "downloaded"
    And the staging slot holds the pack and is erased past it
    And the drive reports the staged version "2.3.4"
    And the drive reports the version of its build and its board kind

  @REQ-UPD-011 @sil
  Scenario: A staged pack waits across runs and restarts
    When the target boots
    And a pack of version "2.3.4" is downloaded
    And the rotor is aligned
    And the motor is enabled
    And the motor is disabled
    And the target restarts
    Then the drive reports the staged version "2.3.4"
    And the journal reads "downloaded"

  @REQ-UPD-002 @sil
  Scenario: A download is refused while the motor runs
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And Begin Upgrade is sent for a pack of version "2.3.4"
    Then Begin Upgrade is answered "busy"
    And the journal reads "empty"

  @REQ-UPD-005 @sil
  Scenario: The apply is refused while the motor runs
    When the target boots
    And a pack of version "2.3.4" is downloaded
    And the rotor is aligned
    And the motor is enabled
    And Activate is sent
    Then Activate is answered "busy"
    And the journal reads "downloaded"

  @REQ-UPD-003 @REQ-UPD-006 @sil
  Scenario: The drive stays stopped while a pack is received
    When the target boots
    And the rotor is aligned
    And Begin Upgrade is sent for a pack of version "2.3.4"
    And the first 10 blocks are sent
    Then the motor shall refuse to enable
    And setting the telemetry rate to 200 Hz is refused as busy
    When Abort is sent
    Then the journal reads "empty"
    When the telemetry rate is set to 200 Hz
    And the motor is enabled

  @REQ-UPD-004 @REQ-UPD-006 @sil
  Scenario: A pack for another board is refused
    When the target boots
    And a pack of version "2.3.4" for another board is downloaded
    Then the download is answered "not ready"
    And the journal reads "empty"
    And the drive reports the staged version "0.0.0"

  @REQ-UPD-004 @REQ-UPD-006 @sil
  Scenario: A pack with a wrong CRC is refused
    When the target boots
    And a pack of version "2.3.4" is downloaded with a CRC that does not match
    Then the download is answered "CRC mismatch"
    And the journal reads "empty"

  @REQ-UPD-014 @sil
  Scenario: The emergency stop ends a download
    When the target boots
    And Begin Upgrade is sent for a pack of version "2.3.4"
    And the first 10 blocks are sent
    And the emergency stop is sent
    Then the next block is answered "invalid state"
    And the journal reads "empty"

  @REQ-UPD-005 @REQ-UPD-007 @REQ-UPD-011 @sil
  Scenario: The apply installs the staged pack
    Given the application is staged as a downloaded pack of the version it runs
    When the target boots
    Then the drive reports the staged version of its build
    When Activate is sent
    Then Activate is answered "ok"
    And the bootloader installs the pack and the application starts again
    And the drive reports the staged version "0.0.0"
    And the drive reports the version of its build and its board kind
