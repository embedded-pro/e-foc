Feature: Firmware Identity
  A CAN client asks the motor controller which firmware it runs and on which board
  before it offers an update. The controller answers with the release version the
  build was configured with and the kind of its board.

  @sil @REQ-FWU-003
  Scenario: The emulated target reports its firmware version and board kind
    Given a nominal motor plant
    When the target boots
    Then the target shall report the firmware version of this build
    And the target shall report board kind 5

  @hil_motor @REQ-FWU-003
  Scenario: The board reports its firmware version and board kind
    Then the drive reports the version of its build and its board kind
