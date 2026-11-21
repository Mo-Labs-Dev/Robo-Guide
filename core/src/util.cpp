#include "robo_guide/core/util.hpp"

#include <cctype>
#include <cstdio>

namespace rg::core {

std::string redactUrl(const std::string& url) {
    const auto scheme = url.find("://");
    if (scheme == std::string::npos) return url;
    const auto authorityStart = scheme + 3;
    auto authorityEnd = url.find_first_of("/?#", authorityStart);
    if (authorityEnd == std::string::npos) authorityEnd = url.size();
    const auto at = url.rfind('@', authorityEnd);
    if (at == std::string::npos || at < authorityStart) return url;
    const auto colon = url.find(':', authorityStart);
    if (colon == std::string::npos || colon > at) return url;  // user only, no password
    return url.substr(0, colon + 1) + "***" + url.substr(at);
}

std::string encodeUserInfo(const std::string& raw) {
    std::string out;
    for (unsigned char c : raw) {
        if (std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            out += buf;
        }
    }
    return out;
}

std::string defaultXrayStatus(Verdict v, XrayDeployment d) {
    const bool cloud = d == XrayDeployment::Cloud;
    switch (v) {
        case Verdict::Pass: return cloud ? "PASSED" : "PASS";
        case Verdict::Fail: return cloud ? "FAILED" : "FAIL";
        case Verdict::Error: return cloud ? "FAILED" : "ABORTED";
        case Verdict::Blocked: return "TODO";
    }
    return "TODO";
}

}  // namespace rg::core
