Feature: Hardware Watchdog Supervision
  The platform exposes a watchdog that supervises application progress. It is
  off until the application enables it, reports the deadline it is holding,
  and brings the power stage to a safe state when a supervised context stops
  making progress.

  @REQ-EH-008 @REQ-HIL-011 @hil
  Scenario: Watchdog is inactive until the application enables it
    Given the hardware target is connected and responding
    When the watchdog command is sent to the hardware target
    Then the watchdog command reports supervision is disabled

  @REQ-EH-008 @REQ-HIL-011 @hil
  Scenario: Enabling the watchdog reports the deadline it holds
    Given the hardware target is connected and responding
    When the watchdog is enabled with a deadline of 1000 ms
    Then the watchdog command reports a deadline of 1000 ms

  @REQ-EH-009 @REQ-HIL-012 @hil
  Scenario: A stalled supervised context resets the target
    Given the hardware target is connected and responding
    When the watchdog is enabled with a deadline of 1000 ms
    And the watchdog_stall command is sent to the hardware target
    And the hardware target reconnects after reset
    Then the watchdog command reports supervision is disabled
