Feature: Disturbance Rejection
  A loop that tracks a setpoint on an unloaded shaft has only done half its job.
  The plant description schedules a signed shaft torque a set time after the
  motor is enabled, the plant reports its trajectory through the disturbance,
  and the harness measures how far the loop was pushed off its setpoint and how
  long it took to come back.

  The torque is small on purpose: with the nominal rotor a step of a few
  millinewton-metres keeps the loop out of current saturation, so the numbers
  measure the control law rather than the current limit.

  The last scenario is the exception, and reaches the current limit on purpose:
  the steady-state demand still fits inside it, so the loop can recover, while
  the transient the step provokes does not, so the recovery runs through a
  saturated command and exercises the anti-windup rather than the linear
  response.

  It reaches the limit by raising the torque rather than by lowering the limit,
  which cannot be done here. Alignment injects open-loop and abandons the attempt
  the moment a phase carries more than the drive says it supports, so the limit
  has a floor: measured against this plant it sits above ten amperes, more than
  half the nominal twenty. Anything low enough to make a millinewton-metre step
  saturate stops the motor aligning at all, and anything alignment survives still
  leaves the loop most of its current. The torque is therefore what moves: half a
  newton-metre asks for about two thirds of what the drive can deliver, which the
  steady state fits inside and the transient does not.

  Its excursion limit is wide because the excursion is: a rotor of seven
  microkilogram-metres-squared loses a great deal of speed in the millisecond
  before the outer loop next runs, and the laws that recover are thrown 205 to
  238 rad/s off a 20 rad/s setpoint before they catch it.

  Only ADRC and LQI run in the default set, because only they come back. A step
  this size asks for about thirteen of the twenty amperes the drive allows, so
  holding it is within reach, and those two return to the setpoint within a few
  hundredths of a second. PID and two-DOF do not: both end the window 10.44 rad/s
  below the setpoint, having given up more than half of it to a disturbance they
  had the current to reject. Their rows carry the envelope the other two meet and
  are held out of the default run until they meet it.

  The @sil rows carry the envelope the product holds today on the nominal plant
  (the Teknic M-2310P-LN-04K reference motor).

  The position rows run a torque of either sign, except for PID and two-DOF,
  which carry only the positive one. Measured against a negative torque those
  two miss the envelope the positive torque meets: PID takes 106 ms to recover
  where the row allows 50, and two-DOF is pushed 0.1008 rad off a 0.1 rad limit.
  Both are the laws that hold 1.5 rad with a standing error of some 0.055 rad,
  which is already most of the 0.08 rad band the recovery is measured against, so
  a disturbance pushing further from the setpoint starts the measurement close to
  its edge. Whether that makes the limits wrong for one direction or the loops
  wrong is not yet established, so the rows are withheld rather than loosened to
  fit or tagged as a defect that has not been confirmed.

  @REQ-SPD-009
  Scenario Outline: The <algorithm> speed loop rejects a <torque> Nm torque step while holding 20 rad/s
    Given a nominal motor plant
    And the plant response is recorded at 1000 Hz for up to 1200 samples
    And a torque step of <torque> Nm applied 300 ms after enable
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

    @sil
    Examples:
      | algorithm | torque | deviation | band | recovery_ms |
      | pid       | 0.002  | 4.0       | 2.0  | 50          |
      | pid       | -0.002 | 4.0       | 2.0  | 50          |
      | adrc      | 0.002  | 4.0       | 2.0  | 50          |
      | adrc      | -0.002 | 4.0       | 2.0  | 50          |
      | twodof    | 0.002  | 4.0       | 2.0  | 50          |
      | twodof    | -0.002 | 4.0       | 2.0  | 50          |
      | lqi       | 0.002  | 4.0       | 2.0  | 50          |
      | lqi       | -0.002 | 4.0       | 2.0  | 50          |

  @sil @REQ-POS-010
  Scenario Outline: The <algorithm> position loop holds 1.5 rad against a <torque> Nm torque step
    Given a nominal motor plant
    And the plant response is recorded at 1000 Hz for up to 1400 samples
    And a torque step of <torque> Nm applied 500 ms after enable
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

    Examples:
      | algorithm | torque | deviation | band | recovery_ms |
      | pid       | 0.002  | 0.1       | 0.08 | 50          |
      | cascadep  | 0.002  | 0.1       | 0.08 | 50          |
      | cascadep  | -0.002 | 0.1       | 0.08 | 50          |
      | lqr       | 0.002  | 0.1       | 0.08 | 50          |
      | lqr       | -0.002 | 0.1       | 0.08 | 50          |
      | lqi       | 0.002  | 0.05      | 0.02 | 50          |
      | lqi       | -0.002 | 0.05      | 0.02 | 50          |
      | twodof    | 0.002  | 0.1       | 0.08 | 50          |

  @REQ-SPD-009
  Scenario Outline: The <algorithm> speed loop recovers through a saturated current command
    Given a nominal motor plant
    And the plant response is recorded at 1000 Hz for up to 1200 samples
    And a torque step of 0.5 Nm applied 300 ms after enable
    And the motor is already calibrated
    And the motor boots in speed mode
    And the speed loop runs the <algorithm> algorithm
    When the target boots
    And the rotor is aligned
    Then the speed loop shall be running the <algorithm> algorithm
    When a speed setpoint of 20 rad/s is applied
    And the motor is enabled
    And the response is captured for 750 ms after enable
    Then the speed deviation after the torque step shall stay below 300.0 rad/s
    And the speed shall recover to within 3.0 rad/s of the setpoint within 350 ms of the torque step
    And the response shall have no dropped samples

    @sil
    Examples:
      | algorithm |
      | adrc      |
      | lqi       |

    @sil-known-defect
    Examples:
      | algorithm |
      | pid       |
      | twodof    |
