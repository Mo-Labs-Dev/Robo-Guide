// Small helpers shared by the app: secret redaction and Xray status mapping.
#pragma once

#include <string>

#include "robo_guide/core/types.hpp"

namespace rg::core {

// Replaces the password in "scheme://user:password@host/..." with "***".
// URLs without credentials are returned unchanged.
std::string redactUrl(const std::string& url);

// Percent-encodes a URL userinfo component (user or password) per RFC 3986.
std::string encodeUserInfo(const std::string& raw);

enum class XrayDeployment { Cloud, DataCenter };

// Default Xray status names. Cloud uses PASSED/FAILED/TODO, Server/DC uses PASS/FAIL/TODO/ABORTED.
// ERROR and BLOCKED are never reported as a pass. Instances with custom statuses can override
// these in config ("xray.statusMap").
std::string defaultXrayStatus(Verdict v, XrayDeployment d);

}  // namespace rg::core
