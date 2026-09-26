Feature: Hardware Watchdog Supervision
  Every MCU platform supervises its event dispatcher with the MCU watchdog
  from boot. When an event-loop action never returns, the watchdog cuts the
  power stage and the hardware resets the target.

  @REQ-EH-008 @REQ-EH-009 @REQ-EH-012 @REQ-HIL-012 @hil
  Scenario: A stalled event loop resets the target and reports the watchdog as the reset cause
    Given the hardware target is connected and responding
    When the watchdog_stall command is sent to the hardware target
    And the hardware target reconnects after reset
    Then the reset_cause command reports Watchdog
