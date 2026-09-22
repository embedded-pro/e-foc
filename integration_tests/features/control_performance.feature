Feature: FOC Control Performance
  Reaching the running state says nothing about how well a loop tracks its
  setpoint. The plant reports its own trajectory on the control-tick time base,
  and the harness measures the step response of each loop: rise time, settling
  time, overshoot, the band the tail still ripples in, and steady-state error.
  The setpoint is applied before the motor is enabled, so enabling is the step
  and the onset is exact.

  The limits below are pinned from a characterisation run against the nominal
  plant (the Teknic M-2310P-LN-04K reference motor) with a margin, and each
  family of laws carries its own row because they are designed to differ:
  two-DOF trades speed for overshoot, deadbeat settles in a sample, sliding mode
  keeps a boundary-layer band.

  The rise_ms and tail_pct columns are not pinned yet: they are deliberately
  permissive, catching only a loop that never rises or never stops ringing,
  until a characterisation run reports what the laws actually measure. Peak time
  is printed on the [METRIC] line but not asserted, because a law that does not
  overshoot peaks wherever its tail ripple happened to be largest.

  The duty cycle reaches the inverter in whole percent, so the current loop
  ripples around its setpoint by about a tenth of an ampere; the @sil current
  rows carry the envelope the product holds today and the @sil-known-defect rows
  the envelope the laws should meet. See documentation/design/software-in-the-loop.md.

  @REQ-SPD-008
  Scenario Outline: The <algorithm> speed loop steps from rest to 20 rad/s
    Given a nominal motor plant
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
    Then the speed step response shall settle into a <band_pct> % band within <settle_ms> ms with overshoot below <overshoot_pct> %
    And the speed step response shall rise within <rise_ms> ms
    And the speed response tail shall stay within <tail_pct> % of the setpoint
    And the steady-state speed error shall be below <error> rad/s
    And the response shall have no dropped samples
    When the motor is disabled
    Then the state machine shall be in the Ready state

    @sil
    Examples:
      | algorithm | band_pct | settle_ms | overshoot_pct | rise_ms | tail_pct | error |
      | pid       | 10       | 60        | 15            | 400     | 25       | 0.5   |
      | adrc      | 10       | 80        | 15            | 400     | 25       | 0.5   |
      | twodof    | 10       | 60        | 15            | 400     | 25       | 0.5   |
      | lqi       | 10       | 60        | 15            | 400     | 25       | 0.5   |

  @REQ-SPD-008
  Scenario Outline: The <algorithm> speed loop follows a setpoint change to <target> rad/s while running
    Given a nominal motor plant
    And the plant response is recorded at 1000 Hz for up to 2000 samples
    And the motor is already calibrated
    And the motor boots in speed mode
    And the speed loop runs the <algorithm> algorithm
    When the target boots
    And the rotor is aligned
    Then the speed loop shall be running the <algorithm> algorithm
    When a speed setpoint of 20 rad/s is applied
    And the motor is enabled
    And the response is captured for 400 ms after enable
    And a speed setpoint of <target> rad/s is applied
    And the response is captured for 550 ms after the last setpoint
    Then the speed step response shall settle into a <band_pct> % band within <settle_ms> ms with overshoot below <overshoot_pct> %
    And the speed step response shall rise within <rise_ms> ms
    And the speed response tail shall stay within <tail_pct> % of the setpoint
    And the steady-state speed error shall be below <error> rad/s
    And the response shall have no dropped samples

    @sil
    Examples:
      | algorithm | target | band_pct | settle_ms | overshoot_pct | rise_ms | tail_pct | error |
      | pid       | 40     | 10       | 60        | 15            | 400     | 25       | 0.5   |
      | pid       | -20    | 10       | 60        | 15            | 400     | 25       | 0.5   |
      | adrc      | 40     | 10       | 80        | 15            | 400     | 25       | 0.5   |
      | adrc      | -20    | 10       | 80        | 15            | 400     | 25       | 0.5   |
      | twodof    | 40     | 10       | 60        | 15            | 400     | 25       | 0.5   |
      | twodof    | -20    | 10       | 60        | 15            | 400     | 25       | 0.5   |
      | lqi       | 40     | 10       | 60        | 15            | 400     | 25       | 0.5   |
      | lqi       | -20    | 10       | 60        | 15            | 400     | 25       | 0.5   |

  @sil @REQ-POS-009
  Scenario Outline: The <algorithm> position loop steps from rest to 1.5 rad
    Given a nominal motor plant
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
    Then the position step response shall settle into a <band_pct> % band within <settle_ms> ms with overshoot below <overshoot_pct> %
    And the position step response shall rise within <rise_ms> ms
    And the position response tail shall stay within <tail_pct> % of the setpoint
    And the steady-state position error shall be below <error> rad
    And the response shall have no dropped samples

    Examples:
      | algorithm | band_pct | settle_ms | overshoot_pct | rise_ms | tail_pct | error |
      | pid       | 10       | 200       | 15            | 400     | 25       | 0.1   |
      | cascadep  | 10       | 200       | 10            | 400     | 25       | 0.02  |
      | lqr       | 10       | 60        | 15            | 400     | 25       | 0.01  |
      | lqi       | 10       | 60        | 50            | 400     | 25       | 0.01  |
      | twodof    | 10       | 300       | 15            | 400     | 25       | 0.1   |

  @sil @REQ-POS-009
  Scenario Outline: The <algorithm> position loop follows a setpoint change to <target> rad while holding
    Given a nominal motor plant
    And the plant response is recorded at 1000 Hz for up to 2000 samples
    And the motor is already calibrated
    And the motor boots in position mode
    And the position loop runs the <algorithm> algorithm
    When the target boots
    And the rotor is aligned
    Then the position loop shall be running the <algorithm> algorithm
    When a position setpoint of 1.5 rad is applied
    And the motor is enabled
    And the response is captured for 400 ms after enable
    And a position setpoint of <target> rad is applied
    And the response is captured for 550 ms after the last setpoint
    Then the position step response shall settle into a <band_pct> % band within <settle_ms> ms with overshoot below <overshoot_pct> %
    And the position step response shall rise within <rise_ms> ms
    And the position response tail shall stay within <tail_pct> % of the setpoint
    And the steady-state position error shall be below <error> rad
    And the response shall have no dropped samples

    Examples:
      | algorithm | target | band_pct | settle_ms | overshoot_pct | rise_ms | tail_pct | error |
      | pid       | -1.5   | 10       | 200       | 15            | 400     | 25       | 0.15  |
      | cascadep  | -1.5   | 10       | 200       | 10            | 400     | 25       | 0.02  |
      | lqr       | -1.5   | 10       | 60        | 15            | 400     | 25       | 0.01  |
      | lqi       | -1.5   | 10       | 60        | 40            | 400     | 25       | 0.01  |
      | twodof    | -1.5   | 10       | 300       | 15            | 400     | 25       | 0.15  |

  @REQ-TRQ-007
  Scenario Outline: The <algorithm> current loop steps from rest to 0.5 A
    Given a nominal motor plant
    And the plant response is recorded at 20000 Hz for up to 400 samples
    And the motor is already calibrated
    And the motor boots in torque mode
    And the current loop runs the <algorithm> algorithm
    When the target boots
    And the rotor is aligned
    Then the current loop shall be running the <algorithm> algorithm
    When a torque setpoint of 0.5 A is applied
    And the motor is enabled
    And the response is captured for 15 ms after enable
    Then the current step response shall settle into a <band_pct> % band within <settle_ms> ms with overshoot below <overshoot_pct> %
    And the current step response shall rise within <rise_ms> ms
    And the current response tail shall stay within <tail_pct> % of the setpoint
    And the steady-state current error shall be below <error> A
    And the response shall have no dropped samples

    @sil
    Examples:
      | algorithm | band_pct | settle_ms | overshoot_pct | rise_ms | tail_pct | error |
      | pid       | 40       | 12        | 20            | 12      | 60       | 0.1   |
      | decoupled | 40       | 12        | 40            | 12      | 60       | 0.1   |
      | deadbeat  | 40       | 5         | 20            | 12      | 60       | 0.1   |
      | sliding   | 40       | 5         | 70            | 12      | 60       | 0.1   |

    @sil-known-defect
    Examples:
      | algorithm | band_pct | settle_ms | overshoot_pct | rise_ms | tail_pct | error |
      | pid       | 10       | 3         | 30            | 2       | 10       | 0.05  |
      | decoupled | 10       | 3         | 30            | 2       | 10       | 0.05  |
      | deadbeat  | 10       | 1         | 30            | 1       | 10       | 0.05  |
      | sliding   | 10       | 3         | 30            | 2       | 10       | 0.05  |

  @REQ-TRQ-007
  Scenario Outline: The <algorithm> current loop follows a setpoint change to <target> A while running
    Given a nominal motor plant
    And the plant response is recorded at 20000 Hz for up to 800 samples
    And the motor is already calibrated
    And the motor boots in torque mode
    And the current loop runs the <algorithm> algorithm
    When the target boots
    And the rotor is aligned
    Then the current loop shall be running the <algorithm> algorithm
    When a torque setpoint of 0.5 A is applied
    And the motor is enabled
    And the response is captured for 15 ms after enable
    And a torque setpoint of <target> A is applied
    And the response is captured for 15 ms after the last setpoint
    Then the current step response shall settle into a <band_pct> % band within <settle_ms> ms with overshoot below <overshoot_pct> %
    And the current step response shall rise within <rise_ms> ms
    And the current response tail shall stay within <tail_pct> % of the setpoint
    And the steady-state current error shall be below <error> A
    And the response shall have no dropped samples

    @sil
    Examples:
      | algorithm | target | band_pct | settle_ms | overshoot_pct | rise_ms | tail_pct | error |
      | pid       | -0.5   | 40       | 12        | 20            | 12      | 60       | 0.1   |
      | pid       | 0.2    | 40       | 12        | 20            | 12      | 60       | 0.1   |
      | decoupled | -0.5   | 40       | 12        | 40            | 12      | 60       | 0.1   |
      | decoupled | 0.2    | 40       | 12        | 40            | 12      | 60       | 0.1   |
      | deadbeat  | -0.5   | 40       | 5         | 20            | 12      | 60       | 0.1   |
      | deadbeat  | 0.2    | 40       | 5         | 20            | 12      | 60       | 0.1   |
      | sliding   | -0.5   | 40       | 5         | 70            | 12      | 60       | 0.1   |
      | sliding   | 0.2    | 40       | 5         | 70            | 12      | 60       | 0.1   |
