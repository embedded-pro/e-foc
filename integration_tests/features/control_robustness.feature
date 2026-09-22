Feature: Control Robustness Against Off-Nominal Plants
  Every envelope in control_performance.feature is measured against the nominal
  plant. That says what the laws do on the motor they were tuned for, and
  nothing about what they do on a noisy encoder, a hot winding, a loaded shaft
  or a motor wound differently. The plant characteristics scenarios cover those
  conditions but only ask whether the rotor turns, which a badly behaved loop
  can satisfy while ringing.

  These scenarios apply the same step and measure the same response against
  each of those plants. A law designed by state feedback carries an extra
  assertion: the firmware is asked which algorithm actually runs, because a
  design that does not converge for a given motor leaves the previous law in
  place rather than failing loudly, and a scenario that only requested one
  would measure the wrong loop.

  The limits are deliberately permissive. They are not a characterisation of
  these plants, which has not been run; they catch a loop that fails to reach
  its setpoint, rings without settling or sits far from it, and they are pinned
  from the [METRIC] lines once measured. See
  documentation/design/software-in-the-loop.md.

  @sil @REQ-CTRL-014 @REQ-SPD-008
  Scenario Outline: The speed loop holds its envelope on a <label> plant
    Given a <preset> motor plant
    And the plant response is recorded at 1000 Hz for up to 800 samples
    And the motor is already calibrated
    And the motor boots in speed mode
    When the target boots
    And the rotor is aligned
    When a speed setpoint of 20 rad/s is applied
    And the motor is enabled
    And the response is captured for 550 ms after enable
    Then the speed step response shall settle into a 10 % band within <settle_ms> ms with overshoot below <overshoot_pct> %
    And the speed step response shall rise within 400 ms
    And the speed response tail shall stay within <tail_pct> % of the setpoint
    And the steady-state speed error shall be below <error> rad/s
    And the response shall have no dropped samples

    Examples:
      | label             | preset    | settle_ms | overshoot_pct | tail_pct | error |
      | noisy             | noisy     | 300       | 60            | 40       | 2.0   |
      | hot               | hot       | 300       | 60            | 40       | 2.0   |
      | mechanically load | loaded    | 300       | 60            | 40       | 2.0   |
      | differently wound | anaheim   | 300       | 60            | 40       | 2.0   |

  @sil @REQ-CTRL-014 @REQ-POS-009
  Scenario Outline: The position loop holds its envelope on a <label> plant
    Given a <preset> motor plant
    And the plant response is recorded at 1000 Hz for up to 800 samples
    And the motor is already calibrated
    And the motor boots in position mode
    When the target boots
    And the rotor is aligned
    When a position setpoint of 1.5 rad is applied
    And the motor is enabled
    And the response is captured for 550 ms after enable
    Then the position step response shall settle into a 10 % band within <settle_ms> ms with overshoot below <overshoot_pct> %
    And the position step response shall rise within 400 ms
    And the position response tail shall stay within <tail_pct> % of the setpoint
    And the steady-state position error shall be below <error> rad
    And the response shall have no dropped samples

    Examples:
      | label             | preset    | settle_ms | overshoot_pct | tail_pct | error |
      | noisy             | noisy     | 400       | 60            | 40       | 0.3   |
      | hot               | hot       | 400       | 60            | 40       | 0.3   |
      | mechanically load | loaded    | 400       | 60            | 40       | 0.3   |
      | differently wound | anaheim   | 400       | 60            | 40       | 0.3   |

  @sil @REQ-CTRL-014
  Scenario Outline: The <algorithm> speed law stays bounded on a <motor> motor
    Given a <motor> motor plant
    And the plant response is recorded at 1000 Hz for up to 800 samples
    And the motor is already calibrated
    And the motor boots in speed mode
    And the speed loop runs the <algorithm> algorithm
    When the target boots
    And the rotor is aligned
    Then the speed loop shall be running the <algorithm> algorithm
    When a speed setpoint of 20 rad/s is applied
    And the motor is enabled
    And the response is captured for 550 ms after enable
    Then the speed step response shall settle into a 10 % band within 300 ms with overshoot below 60 %
    And the speed response tail shall stay within 40 % of the setpoint
    And the steady-state speed error shall be below 2.0 rad/s
    And the response shall have no dropped samples

    Examples:
      | algorithm | motor   |
      | lqi       | anaheim |
      | adrc      | anaheim |
      | twodof    | anaheim |

  @sil @REQ-CTRL-014
  Scenario Outline: The <algorithm> position law stays bounded on a <motor> motor
    Given a <motor> motor plant
    And the plant response is recorded at 1000 Hz for up to 800 samples
    And the motor is already calibrated
    And the motor boots in position mode
    And the position loop runs the <algorithm> algorithm
    When the target boots
    And the rotor is aligned
    Then the position loop shall be running the <algorithm> algorithm
    When a position setpoint of 1.5 rad is applied
    And the motor is enabled
    And the response is captured for 550 ms after enable
    Then the position step response shall settle into a 10 % band within 400 ms with overshoot below 60 %
    And the position response tail shall stay within 40 % of the setpoint
    And the steady-state position error shall be below 0.3 rad
    And the response shall have no dropped samples

    Examples:
      | algorithm | motor   |
      | cascadep  | anaheim |
      | lqr       | anaheim |
      | lqi       | anaheim |
      | twodof    | anaheim |
