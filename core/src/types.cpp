#include "robo_guide/core/types.hpp"

#include <algorithm>
#include <cctype>

namespace rg::core {

const char* toString(Outcome o) {
    switch (o) {
        case Outcome::AccessGranted: return "ACCESS_GRANTED";
        case Outcome::AccessDenied: return "ACCESS_DENIED";
        case Outcome::CredentialRecognized: return "CREDENTIAL_RECOGNIZED";
        case Outcome::CredentialUnknown: return "CREDENTIAL_UNKNOWN";
        case Outcome::CommunicationError: return "COMMUNICATION_ERROR";
        case Outcome::NoEvent: return "NO_EVENT";
    }
    return "NO_EVENT";
}

const char* toString(Verdict v) {
    switch (v) {
        case Verdict::Pass: return "PASS";
        case Verdict::Fail: return "FAIL";
        case Verdict::Error: return "ERROR";
        case Verdict::Blocked: return "BLOCKED";
    }
    return "ERROR";
}

std::optional<Outcome> parseOutcome(const std::string& text) {
    std::string t;
    t.reserve(text.size());
    for (char c : text) {
        if (c == ' ' || c == '-') c = '_';
        t.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    if (t == "ACCESS_GRANTED" || t == "GRANTED") return Outcome::AccessGranted;
    if (t == "ACCESS_DENIED" || t == "DENIED") return Outcome::AccessDenied;
    if (t == "CREDENTIAL_RECOGNIZED" || t == "RECOGNIZED") return Outcome::CredentialRecognized;
    if (t == "CREDENTIAL_UNKNOWN" || t == "UNKNOWN") return Outcome::CredentialUnknown;
    if (t == "COMMUNICATION_ERROR" || t == "COMM_ERROR") return Outcome::CommunicationError;
    if (t == "NO_EVENT" || t == "NONE") return Outcome::NoEvent;
    return std::nullopt;
}

}  // namespace rg::core
