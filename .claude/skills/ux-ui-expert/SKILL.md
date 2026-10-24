---
name: ux-ui-expert
description: UX/UI review and refactoring for e-foc's desktop tools (CAN Commander, simulator, hardware bridge) and their installers. Use when designing, reviewing or changing anything a person sees or operates under tools/ — Qt Widgets layouts, panels, menus, status/feedback, logging views, safety controls (emergency stop), installers, update notifications, error and issue-reporting flows. Produces a findings list ranked by user impact and, when asked, the refactor itself.
---

# UX/UI expert — e-foc desktop tools

The people using these tools are engineers bench-testing a motor that can spin, heat up and hurt
someone. Every decision is judged first by **safety**, then **state visibility**, then **speed of the
common task**. Visual polish comes last and never at the cost of the first three.

Scope: `tools/` (Qt 6 Widgets on the `ui-cpp` theme), their installers and release packaging. Embedded
code is out of scope — the rules in `AGENTS.md` still apply to everything you touch.

## Process

1. **Map the task flows** before reading widgets: install → launch → connect → identify node → command →
   observe → diagnose a failure → report it → update. Note where each flow can stall or mislead.
2. **Read every widget file** of the tool (`gui/`, `main/`) and the observer/logic interface that feeds it.
3. **Walk the heuristics below**; record each finding with the flow it breaks.
4. **Rank** findings: `SAFETY` > `STATE` > `ERROR` > `FLOW` > `CONSISTENCY` > `POLISH`.
5. When asked to refactor: fix in rank order, keep protocol/logic behaviour unchanged unless the finding
   is the logic, add tests for any non-widget logic, update `documentation/` alongside.

## Heuristics

### Safety controls (SAFETY)

- Emergency stop is reachable whenever a bus is connected — never gated by "busy" or the current tab. It
  has a keyboard shortcut, the `ButtonRole::EmergencyStop` style and stays in view. With several motors on
  the bus it reaches all of them, and the result names every motor it could not reach.
- One motor's fault must stay visible while another motor's tab is open (overview row, tab status dot).
- Commands that move the motor need an explicit action (button/Enter), never fire on value change while
  typing (`keyboardTracking` off on setpoint spin boxes).
- Setpoint ranges come from the firmware contract (`foc::CommandLimits`, wire scaling), with units in the
  suffix. No hard-coded magic ranges that disagree with the firmware.

### Visibility of system state (STATE)

- Connection, node, contract compatibility, pending command and motor fault are visible at all times
  (status bar, motor header), not only in a scrolling log. Every log line about a motor names the motor.
- A displayed state comes from the device (ack/telemetry), not from what the user last clicked. Show a
  "requested…" state until the acknowledgement arrives.
- Status colour follows `ui::theme::StatusLevel`: `Ok` healthy, `Warning` attention, `Fault` only for real
  faults — "Disconnected" is `Neutral`, not red. Colour is never the only carrier: the text says it too.
- Stale values are marked or cleared on disconnect; placeholders read "—", not "0.000".

### Errors and recovery (ERROR)

- Disabled controls explain why (tooltip or inline hint): not connected / firmware incompatible / waiting
  for response.
- Failures are reported once, in plain language, with the next step ("Check the interface is up:
  `ip link set can0 up type can bitrate 1000000`"). Timeouts and NACKs name the command that failed.
- Modal dialogs only for things that block all further work (missing driver). Everything else is
  non-blocking (status bar, banner, log).
- Every warning/error also goes to the persistent issue log so it survives the session and can be
  attached to a bug report.

### Flow efficiency (FLOW)

- The last interface, bitrate, node and window layout are restored on launch (`QSettings`).
- Enter applies the focused setpoint; Tab order follows the visual order; menus carry mnemonics and
  standard shortcuts (`QKeySequence::Quit`, F1 help).
- Group by task, not by message type. Remove empty tabs/placeholders — an empty page is a dead end.
- High-rate data (raw CAN frames) is opt-in and bounded (`setMaximumBlockCount`); the log has timestamps,
  a level, a filter and Clear/Save.

### Consistency (CONSISTENCY)

- Use the `ui-cpp` theme (`ui::backend::qt::ApplyTheme`, `StyleButton`, `StyleStatusLabel`) and
  `ui::model` forms for numeric read-outs; no ad-hoc stylesheets or colours.
- Title Case for buttons/menus, sentence case for messages. One verb per action ("Apply", not
  "Set"/"Send"/"Apply" mixed). Product name is always **e-foc CAN Commander** (lower-case "e-foc").
- Units: SI with the symbol in the suffix (`A`, `V`, `rad/s`, `rad`, `Ω`, `mH`).

### Polish and accessibility (POLISH)

- Every interactive widget has an accessible name (`setAccessibleName`) or a buddy label.
- Minimum window size keeps every control usable; splitters, not fixed sizes; no clipped text at 125 %
  and 150 % scaling.
- Application and window icon set; About dialog shows version, Qt version, licence, links.

## Distribution, updates and issue reporting

- **Installer**: branded "e-foc CAN Commander"; Start-menu/desktop entry with icon; registers an
  uninstaller; upgrades in place (uninstall-before-install); ships every runtime it needs. Linux `.deb`
  declares its library dependencies instead of bundling them and installs a `.desktop` entry.
- **Updates**: check in the background at start-up, never block launch, fail silently (log only) when
  offline. Notify with a dismissible banner that links to the installer for this OS and the release
  notes. Manual "Check for Updates" always gives an answer. The user can turn the automatic check off.
- **Issue log**: rotating file in the per-user data folder; timestamps in UTC; levels; nothing secret.
  "Report Issue" opens a pre-filled issue in the browser (version, OS, recent log lines) for the user to
  review — the app never sends data on its own. "Open Log Folder" is one click away.

## Qt Widgets checklist

- [ ] Constructor ≤ ~30 lines: split into `BuildMenus`, `BuildLayout`, `ConnectSignals`.
- [ ] `QShortcut` for E-stop with `Qt::WindowShortcut`; no shortcut conflicts.
- [ ] `QSettings` for preferences and geometry (`saveGeometry`/`saveState`); restored before `show()`.
- [ ] No unbounded widget growth (log views, tables).
- [ ] Observer callbacks only update widgets; no business logic in `gui/`.
- [ ] Child widgets parented; nothing leaks on teardown (see `TelemetryPanel` destructor note).

## Output format (review)

```text
### tools/<tool>/gui/<File>.cpp
SAFETY      — [S1] E-stop disabled while a command is pending (CommandPanel::SetCommandsEnabled). Fix: …
STATE       — [T1] …
ERROR       — [E1] …
FLOW        — [F1] …
CONSISTENCY — [C1] …
POLISH      — [P1] …

Verdict: N safety, N state, … — ship / fix first.
```
