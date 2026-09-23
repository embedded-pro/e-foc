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

  The rise_ms and tail_pct columns were left permissive when they were added and
  are now pinned, each to twice the worst value six repeated runs of this suite
  measured for its row, rounded up. Twice rather than closer because a run
  reproduces its own numbers but not another run's: a setpoint change made while
  running is timed from a command the host delivers, so the tick it lands on
  moves, and the machine that runs continuous integration does not measure quite
  what a development machine measures. Peak time is printed on the [METRIC] line
  but not asserted, because a law that does not overshoot peaks wherever its tail
  ripple happened to be largest. The tail band is a percentage of the step, not
  of the setpoint the step ends at; the two are the same from rest and differ on
  a change made while running.

  The current loop carries only one setpoint-change row where the outer loops
  carry eight, and records it far more slowly than it records a step from rest.
  A command sent while the motor runs is delivered about half a second of guest
  time after enable, so the recording has to run that long before the window it
  measures even opens, and the rate it can run at is bounded: the interrupt
  writes one sample per tick into a ring of a thousand, the event loop empties
  sixteen of them every millisecond, and a recording that produces more than it
  drains is only safe for as long as the ring can absorb the difference. At the
  control rate that is about a quarter of a second, which is ample for a step
  measured from rest and nowhere near enough for one measured half a second in.

  A quarter of the control rate produces well under what the loop drains, so the
  recording is bounded by its own budget rather than by the ring. The window is
  a fixed number of samples, so a slower rate stretches the time it covers and
  the capture after the setpoint has to stretch with it; the resolution left is
  a fifth of a millisecond, against a transient that rises in under two.

  That row carries about twice the steady-state error of the rows measured from
  rest, and an overshoot that will not sit still. Half a second of torque leaves
  the rotor turning, so holding a reversed current against the back-EMF it now
  generates needs a duty the inverter can only approximate in whole percent: the
  standing error lands near a quarter of the setpoint rather than a tenth of it,
  and the overshoot, a steady 8.5 percent when the same law starts from rest,
  wanders between 9 and 15 percent. That is the same duty resolution the band on
  these rows is already wide for, measured at a harder operating point.

  Its tail band moves further than any other measurement here: eleven points,
  between 20 and 31 percent, across runs that differ in nothing but when the
  command arrived. Twice the worst of those is 62, and its limit is rounded up
  past that to 65, so that a spread already this wide cannot spend the margin the
  limit exists to provide.

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
    And the speed response tail shall stay within <tail_pct> % of the step
    And the steady-state speed error shall be below <error> rad/s
    And the response shall have no dropped samples
    When the motor is disabled
    Then the state machine shall be in the Ready state

    @sil
    Examples:
      | algorithm | band_pct | settle_ms | overshoot_pct | rise_ms | tail_pct | error |
      | pid       | 10       | 60        | 15            | 20      | 6        | 0.5   |
      | adrc      | 10       | 80        | 15            | 50      | 6        | 0.5   |
      | twodof    | 10       | 60        | 15            | 30      | 6        | 0.5   |
      | lqi       | 10       | 60        | 15            | 25      | 6        | 0.5   |

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
    And the speed response tail shall stay within <tail_pct> % of the step
    And the steady-state speed error shall be below <error> rad/s
    And the response shall have no dropped samples

    @sil
    Examples:
      | algorithm | target | band_pct | settle_ms | overshoot_pct | rise_ms | tail_pct | error |
      | pid       | 40     | 10       | 60        | 15            | 25      | 5        | 0.5   |
      | pid       | -20    | 10       | 60        | 15            | 20      | 4        | 0.5   |
      | adrc      | 40     | 10       | 80        | 15            | 50      | 6        | 0.5   |
      | adrc      | -20    | 10       | 80        | 15            | 40      | 4        | 0.5   |
      | twodof    | 40     | 10       | 60        | 15            | 40      | 6        | 0.5   |
      | twodof    | -20    | 10       | 60        | 15            | 30      | 4        | 0.5   |
      | lqi       | 40     | 10       | 60        | 15            | 35      | 6        | 0.5   |
      | lqi       | -20    | 10       | 60        | 15            | 30      | 4        | 0.5   |

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
    And the position response tail shall stay within <tail_pct> % of the step
    And the steady-state position error shall be below <error> rad
    And the response shall have no dropped samples

    Examples:
      | algorithm | band_pct | settle_ms | overshoot_pct | rise_ms | tail_pct | error |
      | pid       | 10       | 200       | 15            | 185     | 8        | 0.1   |
      | cascadep  | 10       | 200       | 10            | 210     | 1        | 0.02  |
      | lqr       | 10       | 60        | 15            | 30      | 1        | 0.01  |
      | lqi       | 10       | 60        | 50            | 10      | 1        | 0.01  |
      | twodof    | 10       | 300       | 15            | 310     | 8        | 0.1   |

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
    And the position response tail shall stay within <tail_pct> % of the step
    And the steady-state position error shall be below <error> rad
    And the response shall have no dropped samples

    Examples:
      | algorithm | target | band_pct | settle_ms | overshoot_pct | rise_ms | tail_pct | error |
      | pid       | -1.5   | 10       | 200       | 15            | 195     | 6        | 0.15  |
      | cascadep  | -1.5   | 10       | 200       | 10            | 210     | 1        | 0.02  |
      | lqr       | -1.5   | 10       | 60        | 15            | 30      | 1        | 0.01  |
      | lqi       | -1.5   | 10       | 60        | 40            | 15      | 1        | 0.01  |
      | twodof    | -1.5   | 10       | 300       | 15            | 320     | 6        | 0.15  |

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
    And the current response tail shall stay within <tail_pct> % of the step
    And the steady-state current error shall be below <error> A
    And the response shall have no dropped samples

    @sil
    Examples:
      | algorithm | band_pct | settle_ms | overshoot_pct | rise_ms | tail_pct | error |
      | pid       | 40       | 12        | 20            | 2.5     | 60       | 0.1   |
      | decoupled | 40       | 12        | 40            | 2.5     | 50       | 0.1   |
      | deadbeat  | 40       | 5         | 20            | 1       | 30       | 0.1   |
      | sliding   | 40       | 5         | 70            | 1       | 40       | 0.1   |

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
    And the plant response is recorded at 5000 Hz for up to 10000 samples
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
    And the response is captured for 60 ms after the last setpoint
    Then the current step response shall settle into a <band_pct> % band within <settle_ms> ms with overshoot below <overshoot_pct> %
    And the current step response shall rise within <rise_ms> ms
    And the current response tail shall stay within <tail_pct> % of the step
    And the steady-state current error shall be below <error> A
    And the response shall have no dropped samples

    @sil
    Examples:
      | algorithm | target | band_pct | settle_ms | overshoot_pct | rise_ms | tail_pct | error |
      | pid       | -0.5   | 40       | 12        | 30            | 6       | 65       | 0.2   |
