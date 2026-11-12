Feature: Hardware Watchdog Supervision
  Every MCU platform supervises its event dispatcher with the MCU watchdog
  from boot. When an event-loop action never returns, the watchdog cuts the
  power stage and the hardware resets the target.

  @REQ-PLT-001 @hil
  Scenario: A stalled event loop resets the target and reports the watchdog as the reset cause
    Given the hardware target is connected and responding
    When the watchdog_stall command is sent to the hardware target
    And the hardware target reconnects after reset
    Then the reset_cause command reports Watchdog

  @REQ-PLT-001 @sil
  Scenario: A stalled event loop resets the emulated target and reports the watchdog as the reset cause
    Given a nominal motor plant
    And the event loop of the emulated target stalls 2000 ms after it boots
    When the target boots
    Then the emulated target reboots and reports the reset cause Watchdog
