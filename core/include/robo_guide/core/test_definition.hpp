// Approved test definitions and execution planning.
//
// A test definition is the *approved* description of a physical test. Jira/Xray tests are
// bound to definitions explicitly (config/test_bindings.json); free text from Jira or from an
// AI assistant is never executed directly.
#pragma once

#include <string>
#include <vector>

#include "robo_guide/core/types.hpp"

namespace rg::core {

// The closed set of actions Robo Guide knows how to perform.
enum class ActionType {
    Note,               // log a message
    Wait,               // wait durationMs
    PickCard,           // robot: pick credential from its storage slot
    PresentCard,        // robot: hold credential in reader field for durationMs
    ReturnCard,         // robot: put credential back
    CaptureEvidence,    // snapshot all connected cameras
    CheckPlatformEvent, // read platform events and evaluate expected vs actual
};

const char* toString(ActionType a);
bool parseActionType(const std::string& text, ActionType& out);
bool isRobotAction(ActionType a);

struct TestStep {
    ActionType type = ActionType::Note;
    std::string label;
    int durationMs = 0;
};

struct TestDefinition {
    std::string id;            // e.g. "authorized_card_access"
    std::string title;
    std::string credentialId;  // must match the id used by the platform
    std::string readerId;      // must match the id used by the platform
    Outcome expected = Outcome::NoEvent;
    int eventLatencyMs = 3000; // how long after presentation the platform may report
    int clockSkewMs = 1000;    // tolerated clock difference between Robo Guide and platform
    std::vector<TestStep> steps;
};

// How physical robot actions are carried out in this run.
enum class PresentationMode {
    Robot,            // the arm does it (requires the motion gate to be open)
    OperatorAssisted, // a human performs the step when prompted and confirms it
};

struct Capabilities {
    bool motionAllowed = false;        // result of evaluateMotionGate()
    bool platformAvailable = false;    // a platform client is configured and reachable
    bool operatorObservationAllowed = false; // operator may enter the observed outcome
    int camerasConnected = 0;
};

enum class StepStatus { Ready, OperatorAction, Blocked };

struct PlannedStep {
    TestStep step;
    StepStatus status = StepStatus::Ready;
    std::string reason;
};

struct ExecutionPlan {
    bool runnable = false;
    std::vector<PlannedStep> steps;
    std::vector<std::string> problems;  // validation and capability problems
};

// Structural validation (ids present, steps sane, presentation before platform check, ...).
std::vector<std::string> validate(const TestDefinition& def);

// Maps every step to how it will be executed now, or why it cannot be.
ExecutionPlan plan(const TestDefinition& def, const Capabilities& caps, PresentationMode mode);

}  // namespace rg::core
