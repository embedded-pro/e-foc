Feature: Bootloader
  The emulated target starts the way a board with a bootloader starts: the
  bootloader checks the application slot at every reset, installs a staged pack
  the journal marks ready, and starts only an intact application. Every pack is
  built with the release's pack builder and signed with the public development
  key set the emulated bootloader carries.

  Background:
    Given a nominal motor plant

  @REQ-FWU-001 @sil
  Scenario: The application starts through the bootloader
    Given the application slot holds "the application"
    When the emulated target starts
    Then the application sends its heartbeat

  @REQ-FWU-001 @sil
  Scenario: A staged pack is installed
    Given the application slot holds "nothing"
    And a development-signed pack of the application is staged and the journal reads "ready to deploy"
    When the emulated target starts
    Then the application sends its heartbeat
    And the journal reads "deployed"

  @REQ-FWU-002 @sil
  Scenario: An interrupted installation is repeated
    Given the application slot holds "nothing"
    And a development-signed pack of the application is staged and the journal reads "deploy started"
    When the emulated target starts
    Then the application sends its heartbeat
    And the journal reads "deployed"

  @REQ-FWU-001 @sil
  Scenario: A downloaded pack waits for the apply
    Given the application slot holds "the application"
    And a development-signed pack of the application is staged and the journal reads "downloaded"
    When the emulated target starts
    Then the application sends its heartbeat
    And the journal reads "downloaded"

  @REQ-FWU-001 @sil
  Scenario: A pack with a bad signature is refused
    Given the application slot holds "the application"
    And a development-signed pack "with a bad signature" is staged and the journal reads "ready to deploy"
    When the emulated target starts
    Then the application sends its heartbeat
    And the journal reads "invalid" with the error "invalid signature"

  @REQ-FWU-001 @sil
  Scenario: A pack for another product is refused
    Given the application slot holds "the application"
    And a development-signed pack "for another product" is staged and the journal reads "ready to deploy"
    When the emulated target starts
    Then the application sends its heartbeat
    And the journal reads "invalid" with the error "unknown product"

  @REQ-FWU-001 @sil
  Scenario: A pack for another board is refused before the slot is erased
    Given the application slot holds "the application"
    And a development-signed pack "for another board" is staged and the journal reads "ready to deploy"
    When the emulated target starts
    Then the application sends its heartbeat
    And the journal reads "invalid" with the error "wrong target"

  @REQ-FWU-001 @sil
  Scenario: A pack for another destination is refused
    Given the application slot holds "the application"
    And a development-signed pack "for another destination" is staged and the journal reads "ready to deploy"
    When the emulated target starts
    Then the application sends its heartbeat
    And the journal reads "invalid" with the error "wrong destination"

  @REQ-FWU-001 @sil
  Scenario: A damaged application is not started
    Given the application slot holds "a damaged application"
    When the emulated target starts
    Then no heartbeat arrives within 3 s
    And the debug log says "bootloader: halted"

  @REQ-FWU-001 @sil
  Scenario: An empty slot is not started
    Given the application slot holds "nothing"
    When the emulated target starts
    Then no heartbeat arrives within 3 s
    And the debug log says "bootloader: halted"
