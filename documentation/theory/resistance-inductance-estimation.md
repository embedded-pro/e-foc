---
title: "Electrical Parameters Identification — Resistance and Inductance"
type: theory
status: approved
version: 3.0.0
component: "service-electrical-ident"
date: 2026-07-19
---

| Field     | Value                                          |
|-----------|------------------------------------------------|
| Title     | Electrical Parameters Identification — R and L |
| Type      | theory                                         |
| Status    | approved                                       |
| Version   | 3.0.0                                          |
| Component | service-electrical-ident                       |
| Date      | 2026-07-19                                     |

## Overview

FOC performance depends directly on accurate stator resistance $R_s$ and inductance $L_s$. These
parameters are used to:
1. Design the PI current controller gains ($K_p = L_s \omega_{bw}$, $K_i = R_s \omega_{bw}$).
2. Implement feed-forward decoupling of the dq cross-coupling terms.
3. Estimate motor temperature from measured $R_s$ (since $R_s \propto T$).

This identification procedure injects a **high-frequency (HF) sinusoidal voltage** on a fixed stator
axis (the stationary $\alpha$-axis) and recovers $R_s$ and $L_s$ from the amplitude and phase of the
resulting current using **synchronous demodulation**. Unlike a DC-step method, the injection is
zero-mean so it produces **no net torque**, the rotor is not required to be clamped or aligned, and
the demodulation **rejects** any low-frequency back-EMF caused by residual rotor motion.

---

## Prerequisites

| Symbol     | Meaning                                             | Unit    |
|------------|-----------------------------------------------------|---------|
| $R_s$      | Stator resistance per phase                         | Ω       |
| $L_s$      | Stator inductance ($L_d \approx L_q$ for SPMSM)     | H       |
| $A$        | Injected $\alpha$-axis voltage amplitude            | V       |
| $f_{inj}$  | Injection frequency                                 | Hz      |
| $\omega$   | Injection angular frequency = $2\pi f_{inj}$        | rad/s   |
| $I$        | Steady-state current amplitude                      | A       |
| $\varphi$  | Current phase lag                                   | rad     |
| $Z$        | Impedance magnitude $A/I$                            | Ω       |
| $f_s$      | Sampling frequency                                  | Hz      |
| $N$        | Number of accumulated samples (integer periods)     | samples |
| $M$        | Number of measurement injection periods             | —       |

---

## Mathematical Foundation

### 1. Fixed-Axis Injection and the SPMSM Non-Saliency Assumption

The service injects on the stationary $\alpha$-axis with $\beta = 0$:

$$
V_\alpha(t) = A \sin(\omega t), \qquad V_\beta = 0, \qquad \omega = 2\pi f_{inj}
$$

For a **surface-mounted PMSM** the rotor is magnetically non-salient ($L_d \approx L_q = L_s$), so the
inductance seen along any fixed stator axis is the same constant $L_s$ **regardless of rotor angle**.
No rotor alignment is therefore required. The excited-axis voltage equation is

$$
V_\alpha = R_s\, i_\alpha + L_s \frac{di_\alpha}{dt} + e_\alpha(t)
$$

where $e_\alpha(t)$ is the (low-frequency) back-EMF, treated below as an out-of-band disturbance.

**Zero net torque.** A DC field on one axis exerts a constant torque that swings a free rotor,
inducing back-EMF that corrupts a DC measurement. A zero-mean sinusoid has no DC component, exerts no
net torque, and never pumps the rotor — which is why HF injection needs no clamp.

### 2. AC Steady-State Impedance

Ignoring $e_\alpha$, the linear RL circuit driven at $\omega$ has the steady-state solution

$$
i_\alpha(t) = I \sin(\omega t - \varphi), \qquad
Z = \frac{A}{I} = \sqrt{R_s^2 + (\omega L_s)^2}, \qquad
\varphi = \operatorname{atan2}(\omega L_s,\, R_s)
$$

so the resistance and inductance are the real and imaginary parts of the impedance:

$$
R_s = Z \cos\varphi, \qquad \omega L_s = Z \sin\varphi
$$

### 3. Synchronous Demodulation (Online, O(1) Memory)

The measured $\alpha$-axis current (a forward Clarke transform of the three sampled phase currents) is
correlated with sine and cosine references at $\omega$. Over an **integer** number of injection
periods ($N = M \cdot f_s / f_{inj}$ samples), three running sums are accumulated — no per-sample
buffer is stored:

$$
S = \sum_{k} i_\alpha[k]\sin\theta_k, \quad
C = \sum_{k} i_\alpha[k]\cos\theta_k, \quad
\Sigma_2 = \sum_{k} i_\alpha[k]^2, \qquad \theta_k = \omega\,k/f_s \pmod{2\pi}
$$

Using $\langle \sin^2 \rangle = \tfrac12$ and $\langle \sin\theta\cos\theta \rangle = 0$ over integer
periods:

$$
I_{re} = \frac{2S}{N} = I\cos\varphi, \qquad
I_{im} = \frac{2C}{N} = -\,I\sin\varphi, \qquad
D = I_{re}^2 + I_{im}^2 = I^2
$$

### 4. Closed-Form Recovery

Substituting $A = ZI$ and the identities of Section 2:

$$
\boxed{R_s = \frac{A\, I_{re}}{D}}, \qquad
\boxed{L_s = \frac{-\,A\, I_{im}}{\omega\, D}}
$$

Indeed $A I_{re}/D = (ZI)(I\cos\varphi)/I^2 = Z\cos\varphi = R_s$, and
$-A I_{im}/(\omega D) = (ZI)(I\sin\varphi)/(\omega I^2) = Z\sin\varphi/\omega = L_s$.

**Winding topology.** For a Delta connection the terminals measure $\tfrac{2}{3}$ of the per-phase
value for both quantities; the phase values are recovered with $R_\phi = R_{terminal}\,k_\Delta$ and
$L_\phi = L_{terminal}\,k_\Delta$, $k_\Delta = 1.5$.

**Amplitude scaling.** For a center-aligned half-bridge the phase-to-midpoint voltage amplitude is
$\text{modIndex}\cdot V_{dc}/2$, where the $\alpha$ modulation index equals
$\text{injectionVoltagePercent}/100$ (inverse Clarke maps $\alpha$ to phase A one-to-one). The applied
amplitude used in the closed form is $A = \text{modIndex}\cdot V_{dc}/2$. The modulation index is
clamped so that every leg duty stays within a samplable window at the injection peaks.

### 5. Back-EMF Rejection

Because the sums span an **integer** number of injection periods, any spectral component at a
frequency $\neq f_{inj}$ integrates toward zero. Rotor oscillation appears as a $\sim$1–2 Hz back-EMF
(from on-rig logs); at $f_{inj} = 250\,\text{Hz}$ this is $\sim$125× away, so its contribution to
$S$ and $C$ — and hence to $R_s$ and $L_s$ — is negligible. Increasing $M$ lowers the residual
leakage further.

### 6. Fit Quality (Diagnostic Only)

A THD-like residual quantifies how sinusoidal the measured current was:

$$
\text{fitQuality} = \frac{\bigl|\,\Sigma_2 - N I^2/2\,\bigr|}{N I^2/2}
$$

The demodulated fundamental carries energy $N I^2/2$; any excess is distortion or out-of-band
disturbance (0 = perfect sinusoid). This is **reported as a diagnostic only** and does not invalidate
$R_s$ or $L_s$: synchronous demodulation has already rejected out-of-band content from the parameter
estimates, so a raised residual flags a disturbance (rotor motion, saturation, dead-time distortion)
rather than a bad measurement.

### 7. PWM-to-ADC Pipeline Lag Compensation

On real hardware the duty commanded in callback $k$ drives the current that is sampled in callback
$k+1$: the current measured now was produced by a voltage commanded roughly one sample earlier.
Demodulating the sampled current against the **current** injection phase therefore introduces a phase
error

$$
\varepsilon = 2\pi\,\frac{f_{inj}}{f_s}
$$

($\approx 9°$ at the defaults $f_{inj}=250\,\text{Hz}$, $f_s=10\,\text{kHz}$). Uncompensated this biases
the recovered resistance to $R_{meas} = Z\cos(\varphi + \varepsilon)$ instead of $Z\cos\varphi$ — about
$-34\%$ at the reference rig point ($\varphi \approx 64.5°$).

The **applied** voltage keeps using the live injection phase, $V_\alpha = A\sin(\theta_{inj})$. The
**demodulation reference** instead uses the phase that produced the sampled current,

$$
\theta_{demod} = \theta_{inj} - d\cdot\Delta\theta \pmod{2\pi}, \qquad \Delta\theta = 2\pi f_{inj}/f_s
$$

where $d$ = `voltageToCurrentDelaySamples` (default **1**). With $d$ matched to the pipeline depth the
phase error cancels and $R_{meas} = Z\cos\varphi = R_s$. The default of 1 sample fits the async
PWM/ADC hal, but it is **rig-calibrated**: the operator can tune $d$ until the measured $R$ matches a
multimeter DC-resistance reading of the winding.

### 8. Injection-Frequency Selection

Three constraints pull on $f_{inj}$:

1. **Integer samples per period** — $f_{inj}$ must divide $f_s$ so each window is an exact integer
   number of periods. At $f_s = 10\,\text{kHz}$ the valid options are 200 / 250 / 500 Hz
   (50 / 40 / 20 samples/period).
2. **Conditioning and current SNR** — $Z = \sqrt{R_s^2 + (\omega L_s)^2}$ becomes inductance-dominated
   at high $f_{inj}$, shrinking the current and separating $R_s$ poorly. Best conditioning is at
   $\omega L_s \approx (1\text{–}3) R_s$ (i.e. $\varphi \approx 45°\text{–}70°$).
3. **Back-EMF margin** — $f_{inj}$ must sit far above the rotor-oscillation frequency.

For the reference rig ($R_s \approx 1.5\,\Omega$, $L_s \approx 2\,\text{mH}$) these give $\sim$120–350 Hz;
the default is **250 Hz** ($\omega L_s \approx 3.1\,\Omega$, $I \approx 0.3\text{–}0.5\,\text{A}$).

### 9. Pole Pair Estimation

The number of electrical cycles per mechanical revolution equals the number of pole pairs $p$.
An open-loop voltage vector is rotated through a known number of full electrical revolutions and the
total mechanical rotation is measured by the encoder. Over the sweep the total electrical angle spans
$2\pi N_{rev}$ electrical, which corresponds to $2\pi N_{rev}/p$ mechanical, so:

$$
p = \operatorname{round}\!\left(\frac{N_{rev}}{\Delta\theta_{mech,total} / 2\pi}\right)
$$

where $\Delta\theta_{mech,total}$ is the measured total mechanical rotation. This procedure is purely
kinematic and is unchanged by the HF impedance method.

### 10. Complete Identification Sequence

```
1. Compute injection parameters: modIndex = injectionVoltagePercent/100 (clamped),
   omega = 2*pi*f_inj, samples/period = f_s / f_inj (must be integer).

2. Arm current sampling at f_s. Each callback:
     a. Emit V_alpha = A*sin(theta_inj) via inverse Clarke + centered duties (applied phase).
     b. After the warm-up periods, accumulate S, C, sumSq from i_alpha = Clarke.Forward(phases),
        demodulating against theta_demod = theta_inj - d*delta_theta (d = voltageToCurrentDelaySamples).
     c. Advance both phases; stop after (warmup + measurement) periods.

3. I_re = 2S/N, I_im = 2C/N, D = I_re^2 + I_im^2.
   Reject if sqrt(D) is below the min-current floor.

4. R_s = A*I_re/D,  L_s = -A*I_im/(omega*D).
   Apply the Delta winding correction (k = 1.5) when configured.
   Reject if R_s <= 0 or L_s <= 0.

5. Report { R_s, L_s, 0 (offset), fitQuality }.
```

---

## Block Diagrams

### Electrical Identification Signal Flow

```mermaid
graph TD
    A[Inject V_alpha = A sin(wt)\nalpha-axis, beta = 0] --> B[Sample phase currents\ni_alpha = Clarke.Forward]
    B --> C[Warm-up periods:\ndemodulate, discard]
    C --> D[Measurement periods:\naccumulate S, C, sumSq]
    D --> E[I_re = 2S/N, I_im = 2C/N\nD = I_re^2 + I_im^2]
    E --> F{sqrt(D) above\nmin-current floor?}
    F -- no --> G[nullopt]
    F -- yes --> H[R_s = A I_re / D\nL_s = -A I_im / (w D)]
    H --> I[Delta correction\n+ fitQuality diagnostic]
    I --> J[Report R_s, L_s, 0, quality]
```

### AC Steady-State Current — ASCII Approximation

```
V_alpha, i_alpha (normalised)
   │      V_alpha = A sin(wt)
 +1├      .-''-.            .-''-.
   │    .'      '.        .'      '.
   │   /          \      /          \
  0├--/------------\----/------------\----  wt
   │ /              \  /              \
   │'                ''                '
 -1├   i_alpha = I sin(wt - phi)  (lags by phi)

phi = atan2(w Ls, R);  R = Z cos phi;  w Ls = Z sin phi
```

---

## Numerical Properties

| Property             | Value / Condition                                            |
|----------------------|--------------------------------------------------------------|
| Sampling rate        | $f_s = 10\ \text{kHz}$                                       |
| Injection frequency  | default $250\ \text{Hz}$ (must divide $f_s$)                 |
| Injection amplitude  | default $15\%$ modulation, clamped to a safe duty window     |
| Warm-up periods      | $10$ (AC transient decay before accumulation)               |
| Measurement periods  | $50$ (integer — sets demodulation window $N$)               |
| PWM→ADC delay        | `voltageToCurrentDelaySamples` default $1$ (rig-calibrated)  |
| Memory               | O(1): three float accumulators, no sample buffer            |
| Min-current floor    | $\approx 0.05\ \text{A}$ demodulated magnitude               |
| $R_s$ / $L_s$ recovery | closed form from in-phase / quadrature current             |
| Fit-quality          | THD-like residual, reported as diagnostic (not a gate)      |

### Sensitivity Analysis

| Source of Error         | Effect on $R_s$                                                                                                                       | Effect on $L_s$                            |
|-------------------------|---------------------------------------------------------------------------------------------------------------------------------------|--------------------------------------------|
| Low-frequency back-EMF  | Rejected by integer-period demodulation                                                                                               | Rejected by integer-period demodulation    |
| ADC current offset (DC) | Rejected (orthogonal to $\sin/\cos$)                                                                                                  | Rejected (orthogonal to $\sin/\cos$)       |
| PWM→ADC pipeline lag    | $\varepsilon = 2\pi f_{inj}/f_s$ biases $R$ ($\cos(\varphi+\varepsilon)/\cos\varphi$); compensated via `voltageToCurrentDelaySamples` | Compensated with the same lagged reference |
| Inverter dead-time      | Small apparent-$R$ bias (in phase)                                                                                                    | Indirect                                   |
| $V_{dc}$ variation      | Biases $A$ (kept brief to limit drift)                                                                                                | Biases $A$                                 |
| High $f_{inj}$          | Poor $R_s$ separation (low current)                                                                                                   | Well conditioned                           |
| Magnetic saturation     | $R_s$ / $L_s$ underestimated (nonlinear)                                                                                              | $L_s$ underestimated (nonlinear)           |
| Rotor saliency (IPMSM)  | Axis-dependent $L$ — not separated                                                                                                    | Reports a single blended $L_s$             |

---

## Worked Example

Motor: $R_s = 1.5\,\Omega$, $L_s = 2\,\text{mH}$, $V_{dc} = 24\,\text{V}$, $f_{inj} = 250\,\text{Hz}$,
injection $15\%$ modulation.

**Applied amplitude:** $A = 0.15 \times 24/2 = 1.8\,\text{V}$.

**Impedance and current:**

$$
\omega = 2\pi \cdot 250 = 1570.8\ \text{rad/s}, \quad
\omega L_s = 3.14\,\Omega, \quad
Z = \sqrt{1.5^2 + 3.14^2} = 3.48\,\Omega, \quad
I = A/Z = 0.517\,\text{A}
$$

**Phase:** $\varphi = \operatorname{atan2}(3.14, 1.5) = 1.126\ \text{rad}\ (64.5°)$.

**Demodulated components:** $I_{re} = I\cos\varphi = 0.223\,\text{A}$,
$I_{im} = -I\sin\varphi = -0.467\,\text{A}$, $D = I^2 = 0.267\,\text{A}^2$.

**Recovery:**

$$
R_s = \frac{1.8 \times 0.223}{0.267} = 1.50\,\Omega, \qquad
L_s = \frac{-1.8 \times (-0.467)}{1570.8 \times 0.267} = 2.0\times10^{-3}\,\text{H}
$$

both matching the true values exactly (the phase lag $\varphi$ carries the $R_s$/$L_s$ split; the
amplitude carries $Z$).

---

## Limitations & Assumptions

- **Assumes**: $L_d \approx L_q$ (surface-mounted PMSM), so a fixed-axis injection sees a constant
  $L_s$ and no alignment is needed. For interior PMSM (IPMSM), separating $L_d$ and $L_q$ requires a
  rotating HF injection or explicit q-axis excitation — this is future work; the current method reports
  a single blended $L_s$.
- **Assumes**: Magnetic linearity (no saturation). The injection amplitude keeps the current below the
  saturation current.
- **Assumes**: $f_{inj}$ divides $f_s$ so the demodulation window is an exact integer number of periods.
- **Does not handle**: Inverter dead-time / voltage-offset identification (a small apparent-$R$ bias
  remains; full dead-time compensation is future work).
- **Does not handle**: Temperature-dependent $R_s$ variation during operation.

## References

1. Rauf, A. et al. — "Online Identification of PMSM Parameters Based on Extended Kalman Filter",
   *IEEE Transactions on Industrial Electronics*, 2019.
2. Underwood, S.J. & Husain, I. — "Online Parameter Estimation and Adaptive Control of PMSM",
   *IEEE Transactions on Industrial Electronics*, 2010.
3. Texas Instruments Application Report SPRABV5 — *Motor Control in Embedded Applications*, 2018.
