# Roadmap and status

## Phase 1: Application foundation ✅ (this delivery)
- [x] Keep the working C++/Qt/ROS 2 robot link
- [x] Visible name **Robo Guide**
- [x] Modular source layout, core library with unit tests
- [x] Four RTSP camera feeds with status, snapshots and recording
- [x] Persistent settings, secrets via environment, diagnostics

## Phase 2: Robot model and vision (next)
Suggested order:
1. **Robot model.** Start from Waveshare's official ROS 2 workspace for the RoArm-M2-S
   (`roarm_ws_em0`: `roarm_description` URDF, `roarm_moveit` configuration). It targets ROS 2
   Humble and a USB-serial driver. Port the description and MoveIt config to Jazzy, then **measure
   and validate** link lengths, joint limits and the gripper against the real arm.
2. **MoveIt 2 in simulation:** pickup and reader-presentation trajectories with collision objects
   for the table, card holder and readers.
3. **Camera calibration:** intrinsics for each camera (OpenCV chessboard/ChArUco), then a fixed
   ArUco board in the workspace to get each camera's pose in the robot base frame.
4. **Card and reader localization:** start with ArUco markers on card holders and reader mounts.
   It is reliable and needs no training data. Move to learned detection only where markers are not
   practical.
5. **Pickup verification:** check the gripper camera view and the gripper joint feedback.

## Phase 3: Controlled physical automation
Hardware E-stop, validated limits, a robot step executor behind the safety gate, pickup and release
tests, presentation against one lab reader, error detection and recovery.

## Phase 4: Platform integration
Approved interface → `PlatformClient` subclass → reader events and device status → correlation
(already implemented).

## Phase 5: Jira/Xray
Search and import are implemented. Remaining: validate against your instance, agree on the status
mapping, and decide whether evidence images or links go to Xray.

## Phase 6: Autonomous end-to-end
Switch the presentation mode from operator-assisted to robot for definitions whose physical steps
are validated.
