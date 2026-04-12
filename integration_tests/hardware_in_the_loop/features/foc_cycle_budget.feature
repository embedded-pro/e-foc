Feature: FOC Loop CPU Budget
  The complete FOC interrupt service routine shall not exceed 75 percent of
  available CPU time per control period.

  # REQ-HIL-007
  Scenario: FOC loop cycle count stays within 75 percent CPU budget
    Given the hardware target is connected and responding
    When the FOC loop CPU utilisation is sampled for one control cycle
    Then the measured utilisation shall not exceed 75 percent of one control period
