Feature: HardFault Recovery
  The error handler captures CPU diagnostic data on a HardFault exception
  and reports it after the subsequent reboot.

  # REQ-HIL-010
  Scenario: Device captures and reports fault data after induced HardFault
    Given the hardware target is connected and responding
    When the force_hardfault command is sent to the hardware target
    And the hardware target reconnects after reset
    Then the fault_status command reports captured fault data
