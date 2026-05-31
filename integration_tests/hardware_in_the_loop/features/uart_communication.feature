Feature: UART Serial Communication
  The hardware target presents a well-formed CLI banner over UART after a reset.

  @REQ-HIL-005
  Scenario: CLI banner is presented over UART after reset
    Given the hardware target is connected and responding
    When the reset command is sent to the hardware target
    And the hardware target is rebooted and emits its banner over UART
    Then the CLI banner shall be well-formed
