// Event correlation and expected-vs-actual evaluation.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "robo_guide/core/types.hpp"

namespace rg::core {

// One access-control event, as normalised by a platform client.
struct PlatformEvent {
    std::string eventId;       // platform's own id, kept for evidence
    std::string readerId;      // reader / device identifier
    std::string credentialId;  // credential identifier; may be empty if the platform could not identify it
    Outcome outcome = Outcome::NoEvent;
    std::int64_t timestampMs = 0;  // UTC epoch milliseconds
};

// The window in which an event caused by our presentation may legitimately appear.
struct CorrelationWindow {
    std::string readerId;
    std::string credentialId;
    std::int64_t startMs = 0;  // presentation start (minus allowed clock skew)
    std::int64_t endMs = 0;    // presentation end + allowed platform latency
};

struct CorrelationResult {
    enum class Kind {
        Matched,    // one consistent outcome found
        NoEvent,    // nothing from this reader in the window
        Ambiguous,  // conflicting outcomes or a foreign credential on the same reader
    };
    Kind kind = Kind::NoEvent;
    Outcome outcome = Outcome::NoEvent;      // valid when kind == Matched
    std::vector<PlatformEvent> considered;   // events from the target reader inside the window
    std::string note;
};

// Picks the events that belong to one test presentation.
// Rules:
//  * only events from window.readerId with startMs <= timestamp <= endMs are considered;
//  * an event naming a *different* credential on the same reader makes the result Ambiguous
//    (someone else used the reader, or the wrong card was presented);
//  * events with an empty credential id are accepted (e.g. unknown cards);
//  * several events with the same outcome collapse to one Matched result;
//  * several different outcomes make the result Ambiguous.
CorrelationResult correlate(const std::vector<PlatformEvent>& events, const CorrelationWindow& window);

struct Evaluation {
    Verdict verdict = Verdict::Error;
    Outcome actual = Outcome::NoEvent;
    std::string reason;
};

// Compares expected and actual outcome.
//  * presentationConfirmed == false  -> Error (we cannot claim anything about the reader).
//  * NoEvent                         -> Pass only if NoEvent was explicitly expected, otherwise Error.
//                                       A missing event is never interpreted as an access denial.
//  * Ambiguous                       -> Error.
//  * CommunicationError, not expected -> Error (infrastructure, requires diagnosis).
//  * otherwise                       -> Pass if equal, Fail if different.
Evaluation evaluate(Outcome expected, const CorrelationResult& actual, bool presentationConfirmed);

}  // namespace rg::core
