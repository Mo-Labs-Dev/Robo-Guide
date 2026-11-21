# Robo Guide

Robotic test automation for access-control hardware: a Waveshare RoArm-M2-S, four Tapo C100
cameras, the company access-management platform and Jira/Xray, combined in one C++17 / Qt 6 /
ROS 2 Jazzy desktop application.

> **Safety:** physical robot motion is disabled. It is locked at build time
> (`ROBO_GUIDE_ENABLE_MOTION=OFF`) and by a runtime safety gate, and the robot driver has no
> motion endpoints. Read [docs/SAFETY.md](docs/SAFETY.md) before you change any of this.

---

## What works today

| Area | Status |
|---|---|
| Robot connection & live joint feedback (read-only, via ROS 2) | ✅ Working (kept from the original project) |
| Simulation mode | ✅ Working |
| 10-module desktop app with the name **Robo Guide** | ✅ Implemented |
| 4× Tapo C100 RTSP cameras (+ optional ESP32-CAM over HTTP): live view, auto-reconnect, snapshots, recording | ✅ Implemented |
| Config file + secrets from environment variables (never in Git) | ✅ Implemented |
| Approved test definitions, plan validation, BLOCKED/UNSUPPORTED handling | ✅ Implemented + unit-tested |
| Event correlation and PASS/FAIL/ERROR/BLOCKED verdicts (a missing event is never a pass) | ✅ Implemented + unit-tested |
| Test execution with **operator-assisted** physical steps (Robo Guide prompts, times, judges, records) | ✅ Implemented |
| Evidence folder per run (result.json, steps.log, camera images) | ✅ Implemented |
| Jira test search (Cloud & Data Center), Xray result import (Cloud & Server/DC) | ✅ Implemented, ⚠️ not yet tested against your instance |
| Motion safety gate (checklist, compile-time lock) | ✅ Implemented — every precondition currently reports "unmet" on purpose |
| Task Designer dry-run (note/wait) | ✅ Kept from the original project |
| Company platform connection (events, device status) | 🔶 Interface + development simulator only; real client waits for approved interface docs |
| Robot model, MoveIt 2 planning, calibration, card detection | ❌ Not started (Phase 2) |
| Robot pickup/presentation, collision avoidance, recovery | ❌ Not started (Phase 3) |

So you can already run the full chain **test case → physical presentation → platform result →
verdict → evidence → Xray**, with a person moving the card. When Phase 2/3 are done, the robot takes
over those steps. Nothing else in the pipeline needs to change.

---

## Complete setup (Windows 11 + WSL2 Ubuntu 24.04)

### 1. ROS 2 Jazzy

You already have this. To check:

```bash
source /opt/ros/jazzy/setup.bash && ros2 doctor --report | head -20
```

On a fresh machine, follow the official Jazzy Debian-package install for Ubuntu 24.04
(`ros-jazzy-ros-base` is enough; `ros-jazzy-desktop` adds RViz).

### 2. Get the code and install dependencies

```bash
cd ~
git clone <your-repo-url> Robo-Guide        # or unzip the delivered archive
cd Robo-Guide
chmod +x scripts/*.sh
./scripts/install_dependencies.sh            # Qt 6, OpenCV, FFmpeg, colcon, ...
```

### 3. Set up the network and cameras

1. **Give every device a fixed IP.** In your router, reserve a fixed address for the robot and for each
   camera. Your `192.168.178.x` network is probably a FRITZ!Box: go to *Heimnetz → Netzwerk →
   device → "Diesem Netzwerkgerät immer die gleiche IPv4-Adresse zuweisen"*.
2. **Create a Tapo camera account.** In the Tapo app, for each camera: *camera → ⚙ → Advanced
   Settings → Camera Account*. This login is separate from your TP-Link cloud login. Use the same
   account on all four cameras to keep things simple.
3. **Configure Robo Guide:**
   ```bash
   cp config/robo_guide.example.json config/robo_guide.json   # set the camera IPs, "enabled": true
   cp config/.env.example config/.env                         # set RG_CAMERA_USER / RG_CAMERA_PASSWORD
   chmod 600 config/.env
   ```
   Both files are git-ignored. Robo Guide reads its config in this order: `$RG_CONFIG`, then the file
   written by the **Settings** page (`~/.config/RoboGuide/Robo Guide/robo_guide.json`), then
   `config/robo_guide.json`, then the example. Once you save in Settings, that file wins.
   **Diagnostics** shows which file is in use.
4. **Test the streams:**
   ```bash
   ./scripts/check_cameras.sh
   ```
   You should see a line such as `OK h264,640,360,...` for each camera. `stream2` is the low-resolution stream. Use it
   for four live views in WSL. Switch a camera to `stream1` (full HD) only when it needs more detail.

**WSL2 networking:** WSL2 reaches LAN devices through NAT, so outgoing RTSP over TCP works. Robo
Guide forces TCP for this reason. If a camera answers in VLC on Windows but not in WSL, turn on
mirrored networking: in `%UserProfile%\.wslconfig` add `[wsl2]` and `networkingMode=mirrored`, then
run `wsl --shutdown`.

### 4. Build

```bash
./scripts/build.sh            # ROS 2 driver + app + unit tests (add --clean for a fresh build)
```

### 5. Run

```bash
./scripts/start_all.sh        # driver in background (logs/driver.log) + GUI
# or in two terminals:
./scripts/start_driver.sh
./scripts/start_gui.sh
```

Then:
1. **Robot Control:** check the IP, pick *Hardware (read only)* and click Connect. Joint values update
   live.
2. **Vision Studio:** click *Start all enabled*.
3. **Diagnostics:** shows the full log, the config file in use and the camera FPS.

### 6. First end-to-end test run (no platform needed)

1. In **Settings**, set the platform client to *SIMULATED*, then save and restart.
   Simulated results are labelled as simulated and are never published.
2. In **Test Case Manager**, select `authorized_card_access` and click *Execute selected
   definition*.
3. In **Autonomous Testing**, click *Run test*. Do each operator prompt (take the card → present it
   → confirm, and so on).
4. Check the verdict, then click *Open evidence folder*. The folder holds `result.json`, `steps.log`
   and the camera images.
5. In **Platform Integration**, change *Next simulated outcome* to `NO_EVENT` and run again. The
   verdict must be **ERROR**, not PASS, because a missing event is never treated as a denial.

With the platform set to *None*, the app asks you to record the outcome you observed at the
reader. Results from these runs are marked `OPERATOR_OBSERVED`.

### 7. Connect Jira / Xray

See [docs/INTEGRATIONS.md](docs/INTEGRATIONS.md). Short version:
- In **Settings**, choose the deployment (Cloud or Data Center) and enter the Jira URL and project key.
- Put your secrets in `config/.env`. For Cloud: `RG_JIRA_USER`, `RG_JIRA_TOKEN`, `RG_XRAY_CLIENT_ID`
  and `RG_XRAY_CLIENT_SECRET`. For Data Center: `RG_JIRA_TOKEN` (a personal access token).
- Map Jira test keys to approved definitions: `cp config/test_bindings.example.json
  config/test_bindings.json`, then edit it.
- In **Test Case Manager**, click *Fetch tests from Jira*. Tests with no mapping show as UNSUPPORTED
  and are never executed.

### 8. Git push authentication

The original notes say pushing was still failing. The simplest fix on WSL:

```bash
sudo apt install gh && gh auth login     # choose GitHub.com → HTTPS → "Login with a web browser"
gh auth setup-git
git push
```

Or use an SSH key: `ssh-keygen -t ed25519`, add `~/.ssh/id_ed25519.pub` to GitHub, then
`git remote set-url origin git@github.com:<user>/<repo>.git`.

---

## Project layout

```
CMakeLists.txt               top-level build (core + app); ROBO_GUIDE_ENABLE_MOTION=OFF
core/                        pure C++17 logic, no Qt/ROS, unit-tested
  include/robo_guide/core/   verdict.hpp, safety_gate.hpp, test_definition.hpp, util.hpp, types.hpp
  tests/core_tests.cpp
desktop/robo_guide/src/      Qt 6 application
  RosBridge                  ROS 2 node for the GUI (topics unchanged)
  CameraThread/Manager       OpenCV/FFmpeg RTSP capture, snapshots, recording
  PlatformClient             platform interface + simulator (real client goes here)
  JiraXrayClient             Jira search + Xray import
  TestLibrary/TestRunner     approved definitions, execution state machine
  ExecutionRecord            evidence + Xray JSON
  pages/                     the 10 modules
ros2_ws/src/robotest_waveshare   read-only RoArm driver (unchanged)
ros2_ws/src/robotest_core        old placeholder status node (not built; see note below)
config/                      example config, .env.example, test definitions, bindings
scripts/                     install, build, start, camera check
legacy/                      the original RoboTest Studio GUI, for reference (not built)
docs/                        architecture, safety, integrations, roadmap
```

**Notes on the existing code:**
- The ROS package and topic names (`robotest_waveshare`, `/robotest/robot/*`) are unchanged so the
  working driver keeps working. Renaming them can happen later in one commit.
- `robotest_core/robot_status_node` publishes a fixed "DISCONNECTED" message on the same status
  topic as the driver. Do not run it alongside the driver. It is left out of the build.
- The `.backup` copies and the `qt_app.backup` folder were removed. The original GUI is in
  `legacy/`, and Git history still has everything.

## Troubleshooting

| Symptom | Fix |
|---|---|
| `DRIVER STATUS TIMEOUT` | The driver isn't running. Start `scripts/start_driver.sh` or use `start_all.sh`. |
| `HARDWARE \| FEEDBACK UNAVAILABLE` | Robot not reachable. Run `curl "http://192.168.178.31/js?json=%7B%22T%22%3A105%7D"` from WSL. |
| Camera stuck on "Unavailable - retrying" | Run `scripts/check_cameras.sh`. Check the Tapo camera account, the IP, and that RTSP works in VLC on Windows. |
| `401 Unauthorized` from a camera | You used the TP-Link cloud login. Use the Tapo *Camera Account* instead. |
| Blank or black window in WSLg | `start_gui.sh` already forces software OpenGL. Update WSL with `wsl --update`. |
| CMake can't find `rclcpp` | Run `source /opt/ros/jazzy/setup.bash` before `cmake`, or use `scripts/build.sh`. |
| Jira `HTTP 410` / "API has been removed" | You're on Cloud but configured `datacenter`. Cloud must use `/rest/api/3/search/jql`. |
