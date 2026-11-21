// Motion safety gate. Physical motion is only allowed when *every* precondition holds.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace rg::core {

// Compile-time master switch. The build defines ROBO_GUIDE_ENABLE_MOTION=0 by default, so the
// gate can never open in a default build regardless of runtime state.
#ifndef ROBO_GUIDE_ENABLE_MOTION
#define ROBO_GUIDE_ENABLE_MOTION 0
#endif

struct JointLimit {
    std::string name;
    double minRad = 0.0;
    double maxRad = 0.0;
    bool configured = false;
};

// Joint order matches the Waveshare driver: base, shoulder, elbow, hand.
using JointLimits = std::array<JointLimit, 4>;
using JointVector = std::array<double, 4>;

struct SafetyInputs {
    bool calibrationValid = false;      // camera-to-robot calibration loaded and verified
    bool jointLimitsValidated = false;  // limits measured on the real arm and signed off
    bool emergencyStopTested = false;   // E-stop tested in this session
    bool workspaceClear = false;        // no unexpected obstacles (vision or operator)
    bool operatorArmed = false;         // operator explicitly enabled motion for this run
    bool driverHealthy = false;         // driver connected in hardware mode
    std::int64_t feedbackAgeMs = -1;    // age of the newest joint feedback; <0 means none
    std::int64_t maxFeedbackAgeMs = 1500;
};

struct GateDecision {
    bool allowed = false;
    std::vector<std::string> unmet;  // human-readable reasons; empty only when allowed
};

GateDecision evaluateMotionGate(const SafetyInputs& in);

// Checks a joint target against configured limits. Unconfigured limits always fail.
GateDecision checkJointTarget(const JointLimits& limits, const JointVector& target);

}  // namespace rg::core
