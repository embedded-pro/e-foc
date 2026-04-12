Feature: UART Serial Communication
  The hardware target responds to commands sent over the UART serial interface.

  # REQ-HIL-005
  Scenario: UART ping command receives a well-formed response within timeout
    Given the hardware target is connected and responding
    When the ping command is sent over UART
    Then a well-formed response shall be received within 100 milliseconds
