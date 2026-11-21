# Safety

Physical motion is disabled, and three independent layers keep it that way:

1. **Driver:** `robotest_waveshare` sends only the read-only feedback command `{"T":105}`. It has no
   motion endpoints.
2. **Build:** `ROBO_GUIDE_ENABLE_MOTION` defaults to `OFF`. With it off, `evaluateMotionGate()` can
   never return `allowed`.
3. **Runtime gate:** even in a motion-enabled build, every precondition must hold at the same time.
   The list is visible under *Robot Control → Motion safety gate*.

| Precondition | How it will be satisfied (not implemented yet) |
|---|---|
| Camera-to-robot calibration validated | Hand-eye calibration procedure; residual error below an agreed limit |
| Joint limits validated on the real arm | Measured limits signed off, stored in config, enforced by `checkJointTarget()` |
| Emergency stop tested this session | Physical E-stop that cuts servo power; tested at session start |
| Workspace confirmed clear | Operator confirmation; later, a vision obstacle check |
| Operator armed motion | Explicit, per-session action |
| Driver connected in hardware mode | Already implemented |
| Fresh joint feedback (< 1.5 s) | Already implemented |

Even with the gate open, the test runner refuses robot steps with "Robot execution of physical steps
is not implemented yet". A validated executor (MoveIt 2 trajectories, speed limits, recovery) is a
Phase 3 item.

## Before anyone sets `ROBO_GUIDE_ENABLE_MOTION=ON`

- [ ] A hardware E-stop that removes servo power, without relying on software, is installed and tested.
- [ ] The robot model, joint limits and gripper geometry are validated against the real arm (Phase 2).
- [ ] Trajectories have been verified in simulation with collision checking (MoveIt 2).
- [ ] The workspace is fenced or marked, and the operator procedure is written down.
- [ ] Speed and force limits are set to the minimum the task needs. (The RoArm-M2 firmware already
      caps gripper torque. Check the version you run.)
- [ ] Someone other than the author has reviewed the motion code and gate changes.
