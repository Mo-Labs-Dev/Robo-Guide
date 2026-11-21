#include "robo_guide/core/test_definition.hpp"

namespace rg::core {

const char* toString(ActionType a) {
    switch (a) {
        case ActionType::Note: return "note";
        case ActionType::Wait: return "wait";
        case ActionType::PickCard: return "pick_card";
        case ActionType::PresentCard: return "present_card";
        case ActionType::ReturnCard: return "return_card";
        case ActionType::CaptureEvidence: return "capture_evidence";
        case ActionType::CheckPlatformEvent: return "check_platform_event";
    }
    return "note";
}

bool parseActionType(const std::string& t, ActionType& out) {
    static const ActionType all[] = {ActionType::Note,        ActionType::Wait,
                                     ActionType::PickCard,    ActionType::PresentCard,
                                     ActionType::ReturnCard,  ActionType::CaptureEvidence,
                                     ActionType::CheckPlatformEvent};
    for (auto a : all) {
        if (t == toString(a)) {
            out = a;
            return true;
        }
    }
    return false;
}

bool isRobotAction(ActionType a) {
    return a == ActionType::PickCard || a == ActionType::PresentCard || a == ActionType::ReturnCard;
}

std::vector<std::string> validate(const TestDefinition& d) {
    std::vector<std::string> p;
    if (d.id.empty()) p.push_back("Definition has no id");
    if (d.readerId.empty()) p.push_back("Definition has no readerId");
    if (d.credentialId.empty()) p.push_back("Definition has no credentialId");
    if (d.steps.empty()) p.push_back("Definition has no steps");
    if (d.eventLatencyMs < 0 || d.eventLatencyMs > 60000) p.push_back("eventLatencyMs must be 0..60000");
    if (d.clockSkewMs < 0 || d.clockSkewMs > 60000) p.push_back("clockSkewMs must be 0..60000");

    int presents = 0, checks = 0, picks = 0, returns = 0;
    bool presentedBeforeCheck = false;
    bool holding = false;
    for (std::size_t i = 0; i < d.steps.size(); ++i) {
        const auto& s = d.steps[i];
        const std::string where = "step " + std::to_string(i + 1) + " (" + toString(s.type) + ")";
        if (s.durationMs < 0 || s.durationMs > 60000) p.push_back(where + ": durationMs must be 0..60000");
        switch (s.type) {
            case ActionType::PickCard:
                ++picks;
                if (holding) p.push_back(where + ": card already held");
                holding = true;
                break;
            case ActionType::PresentCard:
                ++presents;
                if (!holding) p.push_back(where + ": present_card requires pick_card first");
                if (s.durationMs <= 0) p.push_back(where + ": present_card needs durationMs > 0");
                presentedBeforeCheck = true;
                break;
            case ActionType::ReturnCard:
                ++returns;
                if (!holding) p.push_back(where + ": return_card without a held card");
                holding = false;
                break;
            case ActionType::CheckPlatformEvent:
                ++checks;
                if (!presentedBeforeCheck) p.push_back(where + ": check must follow present_card");
                break;
            default:
                break;
        }
    }
    if (presents != 1) p.push_back("Definition must contain exactly one present_card step");
    if (checks != 1) p.push_back("Definition must contain exactly one check_platform_event step");
    if (picks != returns) p.push_back("Every pick_card needs a matching return_card");
    return p;
}

ExecutionPlan plan(const TestDefinition& def, const Capabilities& caps, PresentationMode mode) {
    ExecutionPlan out;
    out.problems = validate(def);
    for (const auto& s : def.steps) {
        PlannedStep ps;
        ps.step = s;
        if (isRobotAction(s.type)) {
            if (mode == PresentationMode::Robot) {
                if (caps.motionAllowed) {
                    ps.status = StepStatus::Ready;
                } else {
                    ps.status = StepStatus::Blocked;
                    ps.reason = "Robot motion is locked by the safety gate";
                }
            } else {
                ps.status = StepStatus::OperatorAction;
                ps.reason = "Operator performs this step and confirms it";
            }
        } else if (s.type == ActionType::CheckPlatformEvent) {
            if (caps.platformAvailable) {
                ps.status = StepStatus::Ready;
            } else if (caps.operatorObservationAllowed) {
                ps.status = StepStatus::OperatorAction;
                ps.reason = "No platform connection: operator records the observed outcome";
            } else {
                ps.status = StepStatus::Blocked;
                ps.reason = "No platform connection configured";
            }
        } else if (s.type == ActionType::CaptureEvidence && caps.camerasConnected == 0) {
            ps.status = StepStatus::Ready;
            ps.reason = "No camera connected - evidence will contain logs only";
        }
        if (ps.status == StepStatus::Blocked) out.problems.push_back(std::string(toString(s.type)) + ": " + ps.reason);
        out.steps.push_back(ps);
    }
    out.runnable = out.problems.empty();
    return out;
}

}  // namespace rg::core
