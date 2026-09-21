Feature: Disturbance Rejection
  A loop that tracks a setpoint on an unloaded shaft has only done half its job.
  The plant description schedules a signed shaft torque a set time after the
  motor is enabled, the plant reports its trajectory through the disturbance,
  and the harness measures how far the loop was pushed off its setpoint and how
  long it took to come back.

  The torque is small on purpose: with the nominal rotor a step of a few
  millinewton-metres keeps the loop out of current saturation, so the numbers
  measure the control law rather than the current limit.

  The @sil rows carry the envelope the product holds today, limit cycle
  included; the LQI position row is held out under @sil-known-defect because
  that law is unstable on the nominal plant.

  @sil @REQ-SPD-009
  Scenario Outline: The <algorithm> speed loop rejects a torque step while holding 20 rad/s
    Given a nominal motor plant
    And the plant response is recorded at 1000 Hz for up to 1200 samples
    And a torque step of 0.002 Nm applied 300 ms after enable
    And the motor is already calibrated
    And the motor boots in speed mode
    And the speed loop runs the <algorithm> algorithm
    When the target boots
    And the rotor is aligned
    Then the speed loop shall be running the <algorithm> algorithm
    When a speed setpoint of 20 rad/s is applied
    And the motor is enabled
    And the response is captured for 750 ms after enable
    Then the speed deviation after the torque step shall stay below <deviation> rad/s
    And the speed shall recover to within <band> rad/s of the setpoint within <recovery_ms> ms of the torque step
    And the response shall have no dropped samples

    Examples:
      | algorithm | deviation | band | recovery_ms |
      | pid       | 15.0      | 5.0  | 350         |
      | lqi       | 15.0      | 10.0 | 350         |
      | adrc      | 15.0      | 5.0  | 350         |
      | twodof    | 15.0      | 5.0  | 350         |

  @REQ-POS-010
  Scenario Outline: The <algorithm> position loop holds 1.5 rad against a torque step
    Given a nominal motor plant
    And the plant response is recorded at 1000 Hz for up to 1400 samples
    And a torque step of 0.002 Nm applied 500 ms after enable
    And the motor is already calibrated
    And the motor boots in position mode
    And the position loop runs the <algorithm> algorithm
    When the target boots
    And the rotor is aligned
    Then the position loop shall be running the <algorithm> algorithm
    When a position setpoint of 1.5 rad is applied
    And the motor is enabled
    And the response is captured for 950 ms after enable
    Then the position deviation after the torque step shall stay below <deviation> rad
    And the position shall recover to within <band> rad of the setpoint within <recovery_ms> ms of the torque step
    And the response shall have no dropped samples

    @sil
    Examples:
      | algorithm | deviation | band | recovery_ms |
      | pid       | 0.25      | 0.15 | 350         |
      | cascadep  | 0.25      | 0.15 | 350         |
      | lqr       | 0.25      | 0.15 | 350         |
      | twodof    | 0.25      | 0.15 | 350         |

    @sil-known-defect
    Examples:
      | algorithm | deviation | band | recovery_ms |
      | lqi       | 0.2       | 0.05 | 350         |
