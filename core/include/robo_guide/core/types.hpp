// Robo Guide core types. Pure C++17: no Qt, no ROS, so it can be unit-tested anywhere.
#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace rg::core {

// What the access-control platform reported (or did not report) for a credential presentation.
enum class Outcome {
    AccessGranted,
    AccessDenied,
    CredentialRecognized,
    CredentialUnknown,
    CommunicationError,
    NoEvent,
};

// Final test verdict.
//  Pass    - actual outcome matched the expected outcome.
//  Fail    - the system under test produced a different, well-defined outcome.
//  Error   - the result could not be determined (no event, ambiguous events, infrastructure
//            failure, physical step not confirmed). Requires diagnosis; never reported as PASS.
//  Blocked - the test could not run because a required capability is missing or locked.
enum class Verdict { Pass, Fail, Error, Blocked };

const char* toString(Outcome o);
const char* toString(Verdict v);

// Parses the identifiers produced by toString() and a few common aliases
// ("granted", "denied", "unknown", ...). Case-insensitive.
std::optional<Outcome> parseOutcome(const std::string& text);

}  // namespace rg::core
