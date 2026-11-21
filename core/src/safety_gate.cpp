#include "robo_guide/core/safety_gate.hpp"

#include <cmath>

namespace rg::core {

GateDecision evaluateMotionGate(const SafetyInputs& in) {
    GateDecision d;
#if !ROBO_GUIDE_ENABLE_MOTION
    d.unmet.push_back("Motion is disabled at build time (ROBO_GUIDE_ENABLE_MOTION=OFF)");
#endif
    if (!in.calibrationValid) d.unmet.push_back("Camera-to-robot calibration not validated");
    if (!in.jointLimitsValidated) d.unmet.push_back("Joint limits not validated on the real arm");
    if (!in.emergencyStopTested) d.unmet.push_back("Emergency stop not tested this session");
    if (!in.workspaceClear) d.unmet.push_back("Workspace not confirmed clear");
    if (!in.operatorArmed) d.unmet.push_back("Operator has not armed motion");
    if (!in.driverHealthy) d.unmet.push_back("Robot driver not connected in hardware mode");
    if (in.feedbackAgeMs < 0) {
        d.unmet.push_back("No joint feedback received");
    } else if (in.feedbackAgeMs > in.maxFeedbackAgeMs) {
        d.unmet.push_back("Joint feedback is stale");
    }
    d.allowed = d.unmet.empty();
    return d;
}

GateDecision checkJointTarget(const JointLimits& limits, const JointVector& target) {
    GateDecision d;
    for (std::size_t i = 0; i < limits.size(); ++i) {
        const auto& l = limits[i];
        const std::string name = l.name.empty() ? ("joint " + std::to_string(i)) : l.name;
        if (!l.configured || !(l.minRad < l.maxRad)) {
            d.unmet.push_back(name + ": limits not configured");
            continue;
        }
        if (!std::isfinite(target[i])) {
            d.unmet.push_back(name + ": target is not a finite number");
        } else if (target[i] < l.minRad || target[i] > l.maxRad) {
            d.unmet.push_back(name + ": target outside limits");
        }
    }
    d.allowed = d.unmet.empty();
    return d;
}

}  // namespace rg::core
