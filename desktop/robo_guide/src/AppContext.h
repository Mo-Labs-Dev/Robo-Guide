// Services shared by all pages. Owned by MainWindow.
#pragma once

#include "robo_guide/core/safety_gate.hpp"

class AppConfig;
class ActivityLog;
class RosBridge;
class CameraManager;
class PlatformClient;
class JiraXrayClient;
class TestLibrary;
class TestRunner;

struct AppContext {
    AppConfig* config = nullptr;
    ActivityLog* log = nullptr;
    RosBridge* ros = nullptr;
    CameraManager* cameras = nullptr;
    PlatformClient* platform = nullptr;
    JiraXrayClient* jira = nullptr;
    TestLibrary* tests = nullptr;
    TestRunner* runner = nullptr;

    // Current inputs to the motion gate. Calibration, joint-limit validation, E-stop testing,
    // workspace checks and operator arming do not exist yet, so they are reported as unmet.
    rg::core::SafetyInputs safetyInputs() const;
    rg::core::GateDecision motionGate() const;
};
