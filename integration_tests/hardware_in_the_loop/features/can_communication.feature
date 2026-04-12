Feature: CAN Bus Communication
  The hardware target transmits CAN frames in response to received CAN commands.

  # REQ-HIL-006
  Scenario: CAN command triggers a response frame within the latency budget
    Given the hardware target is connected and responding
    When a CAN command is transmitted to the hardware target
    Then a CAN response frame shall be received within 10 milliseconds
