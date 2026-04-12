Feature: PWM Three-Channel Output
  The hardware target generates three-phase complementary PWM signals.

  # REQ-HIL-002
  Scenario: PWM outputs are active at the configured carrier frequency
    Given the hardware target is connected and responding
    When the PWM three-channel output is started at the default frequency
    Then all three PWM channels shall be switching at the configured carrier frequency
