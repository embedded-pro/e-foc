Feature: Parameter Identification Against a Known Plant
  The firmware identifies the motor it drives: offline through the calibration
  procedures, online through the recursive estimators that run with the speed loop.
  Against a simulated plant whose parameters are known exactly, every estimate can
  be compared with the truth, on both reference motors. The limits are pinned from
  measured runs with a margin; the calibration record and the online estimates
  are read from the product's own trace and CAN response.

  The offline procedures hold their envelopes on both motors. The online
  estimators do not: the mechanical estimator publishes an inertia a third low
  on one motor and nothing at all on the other, and the electrical estimator
  cannot observe the resistance with the product's zero d-axis current, so those
  scenarios carry the envelope the estimators should meet and are held out of
  the default run under the known-defect tag. See Part J of
  documentation/design/software-in-the-loop.md.

  @sil @REQ-CAL-012
  Scenario Outline: Electrical identification reports the plant's resistance, inductance and pole pairs (<motor>)
    Given a <motor> motor plant
    And the motor boots in torque mode
    When the target boots
    And electrical identification is run
    Then the identified resistance shall be within 5 % of the plant
    And the identified inductance shall be within 20 % of the plant
    And the identified pole pairs shall match the plant

    Examples:
      | motor   |
      | teknic  |
      | anaheim |

  @sil @REQ-CAL-012
  Scenario Outline: Electrical identification holds up against measurement noise (<motor>)
    Given a <motor> motor plant
    And a motor plant with:
      | adc_noise_sigma_ampere      | 0.05   |
      | adc_bias_ampere_a           | 0.02   |
      | encoder_noise_sigma_radians | 0.0002 |
    And the motor boots in torque mode
    When the target boots
    And electrical identification is run
    Then the identified resistance shall be within 10 % of the plant
    And the identified inductance shall be within 25 % of the plant
    And the identified pole pairs shall match the plant

    Examples:
      | motor   |
      | teknic  |
      | anaheim |

  @sil @REQ-CAL-013
  Scenario Outline: The full calibration identifies the mechanical parameters (<motor>)
    Given a <motor> motor plant
    And the motor is already calibrated
    And the stored calibration is off by:
      | resistance | 1.3 |
      | inductance | 0.7 |
      | inertia    | 2.0 |
      | friction   | 0.5 |
    And the motor boots in speed mode
    When the target boots
    And the full calibration is run from the terminal
    Then the identified resistance shall be within 5 % of the plant
    And the identified inductance shall be within 20 % of the plant
    And the identified pole pairs shall match the plant
    And the identified inertia shall be within 5 % of the plant
    And the identified friction shall be within 10 % of the plant

    Examples:
      | motor   |
      | teknic  |
      | anaheim |

  @sil-known-defect @REQ-CAL-014
  Scenario Outline: The online estimators converge from a wrong seed under excitation (<motor>)
    Given a <motor> motor plant
    And the plant response is recorded at 100 Hz for up to 2000 samples
    And the motor is already calibrated
    And the stored calibration is off by:
      | resistance | 1.3 |
      | inductance | 0.7 |
      | inertia    | 2.0 |
      | friction   | 0.5 |
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And the speed setpoint alternates between 26 and 52 rad/s every 250 ms for 6000 ms
    And the online estimates are read
    Then the online inertia estimate shall be within 20 % of the plant
    And the online friction estimate shall be within 30 % of the plant
    And the online inductance estimate shall be within 15 % of the plant

    Examples:
      | motor   |
      | teknic  |
      | anaheim |

  @sil-known-defect @REQ-CAL-014
  Scenario Outline: The online resistance estimate follows the plant (<motor>)
    Given a <motor> motor plant
    And the plant response is recorded at 100 Hz for up to 2000 samples
    And the motor is already calibrated
    And the stored calibration is off by:
      | resistance | 1.3 |
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And the speed setpoint alternates between 26 and 52 rad/s every 250 ms for 6000 ms
    And the online estimates are read
    Then the online resistance estimate shall be within 10 % of the plant

    Examples:
      | motor   |
      | teknic  |
      | anaheim |

  @sil-known-defect @REQ-CAL-014
  Scenario Outline: A constant shaft torque does not bias the online mechanical estimates (<motor>)
    Given a <motor> motor plant
    And the plant response is recorded at 100 Hz for up to 2000 samples
    And a torque step of 0.01 Nm applied 500 ms after enable
    And the motor is already calibrated
    And the stored calibration is off by:
      | inertia  | 2.0 |
      | friction | 0.5 |
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And the speed setpoint alternates between 26 and 52 rad/s every 250 ms for 6000 ms
    And the online estimates are read
    Then the online inertia estimate shall be within 20 % of the plant
    And the online friction estimate shall be within 30 % of the plant

    Examples:
      | motor   |
      | teknic  |
      | anaheim |

  @sil-known-defect @REQ-CAL-014
  Scenario: Winding heating is tracked by the online resistance estimate
    Given a hot motor plant
    And the plant response is recorded at 100 Hz for up to 2000 samples
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    And the motor is enabled
    And the speed setpoint alternates between 26 and 52 rad/s every 250 ms for 6000 ms
    And the online estimates are read
    Then the online resistance estimate shall be within 10 % of the plant at 90 celsius
