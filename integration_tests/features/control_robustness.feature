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

  The rise and tail limits were left permissive when these scenarios were
  written and are now pinned the same way as the nominal ones: each is twice
  the worst value six repeated runs measured for its row, rounded up. The
  settling and overshoot limits stay as they were, deliberately wide enough to
  cover a whole group of rows rather than to describe any one of them.

  The laws measured on the second motor carry a rise limit where they carried
  none. The harness had always measured their rise time and printed it, and no
  scenario asserted it, so a law that reached its setpoint by crawling there
  passed. See documentation/design/software-in-the-loop.md.

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
    And the speed step response shall rise within <rise_ms> ms
    And the speed response tail shall stay within <tail_pct> % of the step
    And the steady-state speed error shall be below <error> rad/s
    And the response shall have no dropped samples

    Examples:
      | label             | preset    | settle_ms | overshoot_pct | rise_ms | tail_pct | error |
      | noisy             | noisy     | 300       | 60            | 20      | 10       | 2.0   |
      | hot               | hot       | 300       | 60            | 20      | 6        | 2.0   |
      | mechanically load | loaded    | 300       | 60            | 400     | 10       | 2.0   |
      | differently wound | anaheim   | 300       | 60            | 20      | 5        | 2.0   |

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
    And the position step response shall rise within <rise_ms> ms
    And the position response tail shall stay within <tail_pct> % of the step
    And the steady-state position error shall be below <error> rad
    And the response shall have no dropped samples

    Examples:
      | label             | preset    | settle_ms | overshoot_pct | rise_ms | tail_pct | error |
      | noisy             | noisy     | 400       | 60            | 190     | 8        | 0.3   |
      | hot               | hot       | 400       | 60            | 185     | 8        | 0.3   |
      | mechanically load | loaded    | 400       | 60            | 340     | 13       | 0.3   |
      | differently wound | anaheim   | 400       | 60            | 185     | 8        | 0.3   |

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
    And the speed step response shall rise within <rise_ms> ms
    And the speed response tail shall stay within <tail_pct> % of the step
    And the steady-state speed error shall be below 2.0 rad/s
    And the response shall have no dropped samples

    Examples:
      | algorithm | motor   | rise_ms | tail_pct |
      | lqi       | anaheim | 30      | 5        |
      | adrc      | anaheim | 40      | 5        |
      | twodof    | anaheim | 30      | 4        |

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
    And the position step response shall rise within <rise_ms> ms
    And the position response tail shall stay within <tail_pct> % of the step
    And the steady-state position error shall be below 0.3 rad
    And the response shall have no dropped samples

    Examples:
      | algorithm | motor   | rise_ms | tail_pct |
      | cascadep  | anaheim | 210     | 1        |
      | lqr       | anaheim | 30      | 1        |
      | lqi       | anaheim | 12      | 1        |
      | twodof    | anaheim | 310     | 8        |
