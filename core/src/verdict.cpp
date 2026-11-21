#include "robo_guide/core/verdict.hpp"

#include <set>

namespace rg::core {

CorrelationResult correlate(const std::vector<PlatformEvent>& events, const CorrelationWindow& w) {
    CorrelationResult r;
    if (w.readerId.empty()) {
        r.kind = CorrelationResult::Kind::Ambiguous;
        r.note = "Correlation window has no reader id";
        return r;
    }
    if (w.endMs < w.startMs) {
        r.kind = CorrelationResult::Kind::Ambiguous;
        r.note = "Correlation window end is before start";
        return r;
    }

    std::set<int> outcomes;
    bool foreignCredential = false;
    for (const auto& e : events) {
        if (e.readerId != w.readerId) continue;
        if (e.timestampMs < w.startMs || e.timestampMs > w.endMs) continue;
        r.considered.push_back(e);
        if (!e.credentialId.empty() && !w.credentialId.empty() && e.credentialId != w.credentialId) {
            foreignCredential = true;
            continue;
        }
        if (e.outcome != Outcome::NoEvent) outcomes.insert(static_cast<int>(e.outcome));
    }

    if (foreignCredential) {
        r.kind = CorrelationResult::Kind::Ambiguous;
        r.note = "Event for a different credential on the same reader inside the test window";
        return r;
    }
    if (outcomes.empty()) {
        r.kind = CorrelationResult::Kind::NoEvent;
        r.note = r.considered.empty() ? "No event from the reader inside the test window"
                                      : "Only empty events from the reader inside the test window";
        return r;
    }
    if (outcomes.size() > 1) {
        // CredentialRecognized followed by a decision is a normal two-event sequence on some
        // platforms: keep the decision.
        std::set<int> decisions = outcomes;
        decisions.erase(static_cast<int>(Outcome::CredentialRecognized));
        if (decisions.size() == 1) {
            r.kind = CorrelationResult::Kind::Matched;
            r.outcome = static_cast<Outcome>(*decisions.begin());
            r.note = "Recognition event followed by a decision event";
            return r;
        }
        r.kind = CorrelationResult::Kind::Ambiguous;
        r.note = "Conflicting outcomes from the reader inside the test window";
        return r;
    }
    r.kind = CorrelationResult::Kind::Matched;
    r.outcome = static_cast<Outcome>(*outcomes.begin());
    if (r.considered.size() > 1) r.note = "Duplicate events with the same outcome";
    return r;
}

Evaluation evaluate(Outcome expected, const CorrelationResult& actual, bool presentationConfirmed) {
    Evaluation e;
    if (!presentationConfirmed) {
        e.verdict = Verdict::Error;
        e.actual = Outcome::NoEvent;
        e.reason = "Card presentation was not confirmed; reader behaviour cannot be judged";
        return e;
    }
    switch (actual.kind) {
        case CorrelationResult::Kind::Ambiguous:
            e.verdict = Verdict::Error;
            e.actual = Outcome::NoEvent;
            e.reason = "Ambiguous platform events: " + actual.note;
            return e;
        case CorrelationResult::Kind::NoEvent:
            e.actual = Outcome::NoEvent;
            if (expected == Outcome::NoEvent) {
                e.verdict = Verdict::Pass;
                e.reason = "No event, as explicitly expected";
            } else {
                e.verdict = Verdict::Error;
                e.reason = std::string("No event received (expected ") + toString(expected) +
                           "). A missing event is not treated as a denial - requires diagnosis";
            }
            return e;
        case CorrelationResult::Kind::Matched:
            break;
    }
    e.actual = actual.outcome;
    if (actual.outcome == expected) {
        e.verdict = Verdict::Pass;
        e.reason = std::string("Actual ") + toString(actual.outcome) + " matches expected";
        return e;
    }
    if (actual.outcome == Outcome::CommunicationError) {
        e.verdict = Verdict::Error;
        e.reason = std::string("Communication error reported (expected ") + toString(expected) +
                   ") - requires diagnosis";
        return e;
    }
    e.verdict = Verdict::Fail;
    e.reason = std::string("Expected ") + toString(expected) + ", actual " + toString(actual.outcome);
    return e;
}

}  // namespace rg::core
