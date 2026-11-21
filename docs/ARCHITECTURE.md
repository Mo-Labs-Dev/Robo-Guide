# Robo Guide architecture

## Layers

```
┌──────────────────────────── Robo Guide (Qt 6 desktop app) ────────────────────────────┐
│ Dashboard │ Robot Control │ Vision Studio │ Test Case Mgr │ Task Designer │ Autonomous │
│ Platform Integration │ Jira/Xray │ Diagnostics │ Settings                              │
├───────────────────────────────────────────────────────────────────────────────────────┤
│ AppContext services                                                                    │
│  RosBridge   CameraManager   PlatformClient   JiraXrayClient   TestLibrary  TestRunner │
├───────────────────────────────────────────────────────────────────────────────────────┤
│ robo_guide_core (C++17, no Qt/ROS, unit-tested)                                        │
│  correlate() / evaluate()   evaluateMotionGate()   validate() / plan()   redactUrl()   │
└───────────────────────────────────────────────────────────────────────────────────────┘
        │ ROS 2 topics                  │ RTSP/TCP (FFmpeg)        │ HTTPS
 robotest_waveshare driver        4× Tapo C100 (+ESP32-CAM)     Jira / Xray, company platform
        │ HTTP JSON (T:105 feedback only)
 RoArm-M2-S (ESP32)
```

Decisions behind this layout:
- **The logic that decides verdicts and motion permission is in `core/`.** It has no Qt and no ROS,
  so it is covered by unit tests in CI and anyone can review it without a GUI.
- **The robot link is unchanged.** The GUI uses the same topics and JSON command format as the
  original application, so the working driver did not have to be touched.
- **One thread per camera.** Each camera has its own decode thread. The GUI receives about 8 preview
  frames per second, the latest full frame is kept for snapshots, and recording writes MJPG/AVI.

## ROS 2 interface (unchanged)

| Topic | Type | Direction |
|---|---|---|
| `/robotest/robot/connection_command` | `std_msgs/String` (JSON `{"action":"hardware","ip":...}`, `simulation`, `disconnect`) | GUI → driver |
| `/robotest/robot/status` | `std_msgs/String` (`HARDWARE \| CONNECTED`, ...) | driver → GUI |
| `/robotest/robot/joint_states` | `sensor_msgs/JointState` (base, shoulder, elbow, hand) | driver → GUI |

## Test execution pipeline

```
Jira test key ──binding──▶ approved TestDefinition (JSON)
                              │ validate(): ids, step order, exactly one present + one check
                              ▼
                     plan(definition, capabilities, mode)
                              │ each step: Ready / OperatorAction / Blocked
                              │ any Blocked → run recorded as BLOCKED (with evidence)
                              ▼
                        TestRunner state machine
  pick ─▶ present (window start = prompt − clockSkew) ─▶ hold durationMs (window end)
       ─▶ capture evidence ─▶ wait eventLatency ─▶ platform fetch / operator observation
       ─▶ correlate() ─▶ evaluate() ─▶ return ─▶ result.json + steps.log + images
                              ▼
                    Xray JSON (explicit publish only; never for simulated results)
```

### Verdict rules (`core/src/verdict.cpp`)

| Situation | Verdict |
|---|---|
| Card presentation not confirmed | ERROR |
| No event in the window, a decision was expected | ERROR (requires diagnosis) |
| No event, `NO_EVENT` explicitly expected | PASS |
| Event for another credential on the same reader in the window | ERROR (ambiguous) |
| Conflicting decisions | ERROR (ambiguous) |
| `CREDENTIAL_RECOGNIZED` followed by one decision | the decision is used |
| `COMMUNICATION_ERROR` when something else was expected | ERROR |
| Actual == expected | PASS |
| Actual ≠ expected (well-defined outcome) | FAIL |

### Why test definitions are approved files, not free text

The brief says AI may help interpret tests, but robot actions must be checked against approved
definitions. Robo Guide only runs the closed set of actions in `ActionType`. A Jira test runs only when
someone has explicitly bound it to a reviewed definition. A later AI step can *propose* definitions
or bindings, but a person approves them before they enter `config/`.

## Adding the real platform client

1. Get the approved interface documentation and a read-only test account.
2. Subclass `PlatformClient` (see `PlatformClient.h`), map the platform's records to
   `rg::core::PlatformEvent`, and handle timestamps in UTC.
3. Register it in `createPlatformClient()` under a new `platform.mode` value.
4. Nothing else changes: the runner, correlation, verdicts and evidence already use the interface.
