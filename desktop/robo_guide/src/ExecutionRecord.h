// Everything recorded about one test execution; serialised to result.json in the evidence folder.
#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "robo_guide/core/verdict.hpp"

class AppConfig;

struct ExecutionRecord {
    QString executionId;
    QString testId;        // approved definition id
    QString testTitle;
    QString jiraTestKey;   // optional
    QString credentialId;
    QString readerId;
    QString mode;          // "OPERATOR_ASSISTED" | "ROBOT"
    QString resultSource;  // "PLATFORM" | "SIMULATED_PLATFORM" | "OPERATOR_OBSERVED" | "NONE"
    QDateTime started, finished;
    rg::core::Outcome expected = rg::core::Outcome::NoEvent;
    rg::core::Outcome actual = rg::core::Outcome::NoEvent;
    rg::core::Verdict verdict = rg::core::Verdict::Error;
    QString reason;
    bool presentationConfirmed = false;
    qint64 windowStartMs = 0, windowEndMs = 0;
    QVector<rg::core::PlatformEvent> events;
    QStringList stepLog;
    QStringList evidenceFiles;
    QString evidenceDir;

    bool simulated() const { return resultSource == "SIMULATED_PLATFORM"; }
    QJsonObject toJson() const;
};

namespace Evidence {
// Creates <base>/<yyyyMMdd>/<executionId>_<testId>/ and returns it.
QString createExecutionDir(const QString& base, const QString& executionId, const QString& testId);
// Writes result.json and steps.log. Returns false and sets error on failure.
bool write(const ExecutionRecord& r, QString* error);
// Xray JSON import payload. testExecutionKey may be empty (Xray then creates a new execution).
QByteArray buildXrayJson(const ExecutionRecord& r, const AppConfig& cfg, const QString& testExecutionKey,
                         bool attachImages);
}  // namespace Evidence
