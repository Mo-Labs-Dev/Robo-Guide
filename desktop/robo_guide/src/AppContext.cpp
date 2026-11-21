#include "AppContext.h"

#include "RosBridge.h"

rg::core::SafetyInputs AppContext::safetyInputs() const {
    rg::core::SafetyInputs in;
    in.driverHealthy = ros && ros->monitoring() && ros->healthy() && ros->hardwareMode();
    in.feedbackAgeMs = ros ? ros->jointFeedbackAgeMs() : -1;
    // Not implemented yet - deliberately false until the corresponding procedures exist.
    in.calibrationValid = false;
    in.jointLimitsValidated = false;
    in.emergencyStopTested = false;
    in.workspaceClear = false;
    in.operatorArmed = false;
    return in;
}

rg::core::GateDecision AppContext::motionGate() const { return rg::core::evaluateMotionGate(safetyInputs()); }
