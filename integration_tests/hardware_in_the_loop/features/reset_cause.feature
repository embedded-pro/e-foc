Feature: Reset Cause Detection
  The hardware target correctly identifies and reports the cause of the
  most recent reset via the CLI banner and the reset_cause command.

  # REQ-EH-005, REQ-HIL-008
  Scenario: Device reports software reset cause after NVIC reset
    Given the hardware target is connected and responding
    When the reset command is sent to the hardware target
    And the hardware target reconnects after reset
    Then the reset_cause command reports Software


