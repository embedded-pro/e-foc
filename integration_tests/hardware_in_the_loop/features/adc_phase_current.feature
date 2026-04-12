Feature: ADC Phase Current Measurement
  The hardware target reads three-phase motor currents from its ADC peripheral.

  # REQ-HIL-001
  Scenario: ADC phase current readings are within expected range at rest
    Given the hardware target is connected and responding
    When the ADC phase current measurement is requested
    Then the measured phase currents shall be within the idle current range
