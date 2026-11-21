// Dependency-free unit tests for the Robo Guide core. Run with `ctest` or directly.
#include <cstdio>
#include <string>
#include <vector>

#include "robo_guide/core/safety_gate.hpp"
#include "robo_guide/core/test_definition.hpp"
#include "robo_guide/core/util.hpp"
#include "robo_guide/core/verdict.hpp"

using namespace rg::core;

static int g_failures = 0;
static int g_checks = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        ++g_checks;                                                                 \
        if (!(cond)) {                                                              \
            ++g_failures;                                                           \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                           \
    } while (0)

static PlatformEvent ev(const std::string& reader, const std::string& cred, Outcome o, std::int64_t t) {
    PlatformEvent e;
    e.eventId = "e" + std::to_string(t);
    e.readerId = reader;
    e.credentialId = cred;
    e.outcome = o;
    e.timestampMs = t;
    return e;
}

static CorrelationWindow win() {
    CorrelationWindow w;
    w.readerId = "R1";
    w.credentialId = "C1";
    w.startMs = 1000;
    w.endMs = 5000;
    return w;
}

static void testCorrelation() {
    // Matched inside window, ignores other readers and out-of-window events.
    auto r = correlate({ev("R1", "C1", Outcome::AccessGranted, 2000), ev("R2", "C9", Outcome::AccessDenied, 2000),
                        ev("R1", "C1", Outcome::AccessDenied, 9000)},
                       win());
    CHECK(r.kind == CorrelationResult::Kind::Matched);
    CHECK(r.outcome == Outcome::AccessGranted);
    CHECK(r.considered.size() == 1);

    // Nothing in window -> NoEvent.
    r = correlate({ev("R1", "C1", Outcome::AccessGranted, 500)}, win());
    CHECK(r.kind == CorrelationResult::Kind::NoEvent);

    // Foreign credential on same reader -> Ambiguous.
    r = correlate({ev("R1", "C1", Outcome::AccessGranted, 2000), ev("R1", "C2", Outcome::AccessDenied, 2500)}, win());
    CHECK(r.kind == CorrelationResult::Kind::Ambiguous);

    // Conflicting decisions -> Ambiguous.
    r = correlate({ev("R1", "C1", Outcome::AccessGranted, 2000), ev("R1", "C1", Outcome::AccessDenied, 2500)}, win());
    CHECK(r.kind == CorrelationResult::Kind::Ambiguous);

    // Recognition + decision -> decision wins.
    r = correlate({ev("R1", "C1", Outcome::CredentialRecognized, 2000), ev("R1", "C1", Outcome::AccessDenied, 2100)},
                  win());
    CHECK(r.kind == CorrelationResult::Kind::Matched);
    CHECK(r.outcome == Outcome::AccessDenied);

    // Empty credential id (unknown card) is accepted.
    r = correlate({ev("R1", "", Outcome::CredentialUnknown, 2000)}, win());
    CHECK(r.kind == CorrelationResult::Kind::Matched);
    CHECK(r.outcome == Outcome::CredentialUnknown);

    // Duplicates collapse.
    r = correlate({ev("R1", "C1", Outcome::AccessGranted, 2000), ev("R1", "C1", Outcome::AccessGranted, 2001)}, win());
    CHECK(r.kind == CorrelationResult::Kind::Matched);

    // Invalid window.
    auto bad = win();
    bad.readerId.clear();
    CHECK(correlate({}, bad).kind == CorrelationResult::Kind::Ambiguous);
}

static void testEvaluation() {
    CorrelationResult granted;
    granted.kind = CorrelationResult::Kind::Matched;
    granted.outcome = Outcome::AccessGranted;
    CorrelationResult none;
    none.kind = CorrelationResult::Kind::NoEvent;
    CorrelationResult amb;
    amb.kind = CorrelationResult::Kind::Ambiguous;
    CorrelationResult comm;
    comm.kind = CorrelationResult::Kind::Matched;
    comm.outcome = Outcome::CommunicationError;

    // Table from the project brief.
    CHECK(evaluate(Outcome::AccessGranted, granted, true).verdict == Verdict::Pass);
    CHECK(evaluate(Outcome::AccessDenied, granted, true).verdict == Verdict::Fail);  // unknown credential granted
    // Missing event must NOT become a pass for an expected denial.
    CHECK(evaluate(Outcome::AccessDenied, none, true).verdict == Verdict::Error);
    CHECK(evaluate(Outcome::CommunicationError, none, true).verdict == Verdict::Error);  // reader offline row
    CHECK(evaluate(Outcome::NoEvent, none, true).verdict == Verdict::Pass);  // only when explicitly expected
    CHECK(evaluate(Outcome::AccessGranted, amb, true).verdict == Verdict::Error);
    CHECK(evaluate(Outcome::AccessGranted, comm, true).verdict == Verdict::Error);
    CHECK(evaluate(Outcome::CommunicationError, comm, true).verdict == Verdict::Pass);
    // Unconfirmed presentation is always an error, even if the outcome matches.
    CHECK(evaluate(Outcome::AccessGranted, granted, false).verdict == Verdict::Error);
}

static void testSafetyGate() {
    SafetyInputs in;
    auto d = evaluateMotionGate(in);
    CHECK(!d.allowed);
    CHECK(d.unmet.size() >= 7);

    SafetyInputs all;
    all.calibrationValid = all.jointLimitsValidated = all.emergencyStopTested = true;
    all.workspaceClear = all.operatorArmed = all.driverHealthy = true;
    all.feedbackAgeMs = 100;
    d = evaluateMotionGate(all);
#if ROBO_GUIDE_ENABLE_MOTION
    CHECK(d.allowed);
#else
    CHECK(!d.allowed);  // default build: never allowed
    CHECK(d.unmet.size() == 1);
#endif
    all.feedbackAgeMs = 99999;
    CHECK(!evaluateMotionGate(all).allowed);

    JointLimits lim{};
    JointVector target{0, 0, 0, 0};
    CHECK(!checkJointTarget(lim, target).allowed);  // unconfigured
    for (auto& l : lim) {
        l.configured = true;
        l.minRad = -1;
        l.maxRad = 1;
    }
    CHECK(checkJointTarget(lim, target).allowed);
    target[2] = 1.5;
    CHECK(!checkJointTarget(lim, target).allowed);
}

static TestDefinition goodDef() {
    TestDefinition d;
    d.id = "authorized_card_access";
    d.readerId = "R1";
    d.credentialId = "C1";
    d.expected = Outcome::AccessGranted;
    d.steps = {{ActionType::PickCard, "pick", 0},
               {ActionType::PresentCard, "present", 2000},
               {ActionType::CheckPlatformEvent, "check", 0},
               {ActionType::CaptureEvidence, "evidence", 0},
               {ActionType::ReturnCard, "return", 0}};
    return d;
}

static void testDefinitions() {
    CHECK(validate(goodDef()).empty());

    auto bad = goodDef();
    bad.steps.erase(bad.steps.begin());  // no pick before present, unmatched return
    CHECK(!validate(bad).empty());

    bad = goodDef();
    std::swap(bad.steps[1], bad.steps[2]);  // check before present
    CHECK(!validate(bad).empty());

    Capabilities caps;  // nothing available
    auto p = plan(goodDef(), caps, PresentationMode::Robot);
    CHECK(!p.runnable);
    CHECK(p.steps[0].status == StepStatus::Blocked);

    p = plan(goodDef(), caps, PresentationMode::OperatorAssisted);
    CHECK(!p.runnable);  // platform check still blocked
    caps.operatorObservationAllowed = true;
    p = plan(goodDef(), caps, PresentationMode::OperatorAssisted);
    CHECK(p.runnable);
    CHECK(p.steps[0].status == StepStatus::OperatorAction);
    CHECK(p.steps[2].status == StepStatus::OperatorAction);

    caps.platformAvailable = true;
    p = plan(goodDef(), caps, PresentationMode::OperatorAssisted);
    CHECK(p.steps[2].status == StepStatus::Ready);

    ActionType a;
    CHECK(parseActionType("present_card", a) && a == ActionType::PresentCard);
    CHECK(!parseActionType("move_joint", a));  // not an approved action
}

static void testUtil() {
    CHECK(redactUrl("rtsp://cam:s3cret@192.168.1.5:554/stream1") == "rtsp://cam:***@192.168.1.5:554/stream1");
    CHECK(redactUrl("rtsp://192.168.1.5:554/stream1") == "rtsp://192.168.1.5:554/stream1");
    CHECK(redactUrl("rtsp://user@host/x") == "rtsp://user@host/x");
    CHECK(redactUrl("http://host/path?x=a:b@c") == "http://host/path?x=a:b@c");
    CHECK(encodeUserInfo("p@ss:w/rd") == "p%40ss%3Aw%2Frd");
    CHECK(defaultXrayStatus(Verdict::Pass, XrayDeployment::Cloud) == "PASSED");
    CHECK(defaultXrayStatus(Verdict::Fail, XrayDeployment::DataCenter) == "FAIL");
    CHECK(defaultXrayStatus(Verdict::Error, XrayDeployment::Cloud) != "PASSED");
    CHECK(defaultXrayStatus(Verdict::Blocked, XrayDeployment::DataCenter) == "TODO");
    CHECK(parseOutcome("granted") == Outcome::AccessGranted);
    CHECK(parseOutcome("ACCESS-DENIED") == Outcome::AccessDenied);
    CHECK(!parseOutcome("maybe").has_value());
}

int main() {
    testCorrelation();
    testEvaluation();
    testSafetyGate();
    testDefinitions();
    testUtil();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
