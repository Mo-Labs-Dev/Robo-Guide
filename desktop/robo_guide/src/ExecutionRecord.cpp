#include "ExecutionRecord.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTextStream>

#include "AppConfig.h"
#include "robo_guide/core/util.hpp"

using namespace rg::core;

static QString q(const std::string& s) { return QString::fromStdString(s); }

// ISO 8601 with an explicit offset (e.g. 2026-11-21T19:49:00+01:00), as in the Xray examples.
static QString isoWithOffset(const QDateTime& dt) { return dt.toOffsetFromUtc(dt.offsetFromUtc()).toString(Qt::ISODate); }

QJsonObject ExecutionRecord::toJson() const {
    QJsonArray ev;
    for (const auto& e : events) {
        ev.append(QJsonObject{{"eventId", q(e.eventId)},
                              {"readerId", q(e.readerId)},
                              {"credentialId", q(e.credentialId)},
                              {"outcome", toString(e.outcome)},
                              {"timestamp", QDateTime::fromMSecsSinceEpoch(e.timestampMs, Qt::UTC).toString(Qt::ISODateWithMs)}});
    }
    QJsonArray files;
    for (const auto& f : evidenceFiles) files.append(QFileInfo(f).fileName());
    return QJsonObject{
        {"schema", "robo-guide/execution/1"},
        {"executionId", executionId},
        {"testId", testId},
        {"testTitle", testTitle},
        {"jiraTestKey", jiraTestKey},
        {"credentialId", credentialId},
        {"readerId", readerId},
        {"mode", mode},
        {"resultSource", resultSource},
        {"simulated", simulated()},
        {"started", started.toUTC().toString(Qt::ISODateWithMs)},
        {"finished", finished.toUTC().toString(Qt::ISODateWithMs)},
        {"expected", toString(expected)},
        {"actual", toString(actual)},
        {"verdict", toString(verdict)},
        {"reason", reason},
        {"presentationConfirmed", presentationConfirmed},
        {"correlationWindow",
         QJsonObject{{"start", QDateTime::fromMSecsSinceEpoch(windowStartMs, Qt::UTC).toString(Qt::ISODateWithMs)},
                     {"end", QDateTime::fromMSecsSinceEpoch(windowEndMs, Qt::UTC).toString(Qt::ISODateWithMs)}}},
        {"platformEvents", ev},
        {"evidenceFiles", files},
        {"steps", QJsonArray::fromStringList(stepLog)},
    };
}

QString Evidence::createExecutionDir(const QString& base, const QString& executionId, const QString& testId) {
    QString safeTest = testId;
    for (auto& c : safeTest)
        if (!c.isLetterOrNumber() && c != '-' && c != '_') c = '_';
    const QString dir = QDir(base).filePath(QDate::currentDate().toString("yyyyMMdd") + "/" + executionId + "_" + safeTest);
    QDir().mkpath(dir);
    return dir;
}

bool Evidence::write(const ExecutionRecord& r, QString* error) {
    if (r.evidenceDir.isEmpty()) {
        if (error) *error = "No evidence folder";
        return false;
    }
    QSaveFile f(QDir(r.evidenceDir).filePath("result.json"));
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = "Cannot write result.json";
        return false;
    }
    f.write(QJsonDocument(r.toJson()).toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        if (error) *error = "Cannot commit result.json";
        return false;
    }
    QFile log(QDir(r.evidenceDir).filePath("steps.log"));
    if (log.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream s(&log);
        for (const auto& l : r.stepLog) s << l << "\n";
    }
    return true;
}

QByteArray Evidence::buildXrayJson(const ExecutionRecord& r, const AppConfig& cfg, const QString& execKey,
                                   bool attachImages) {
    const bool cloud = cfg.jira.deployment != "datacenter";
    const QString verdictName = QString::fromLatin1(toString(r.verdict));
    QString status = cfg.jira.statusMap.value(verdictName);
    if (status.isEmpty())
        status = QString::fromStdString(
            defaultXrayStatus(r.verdict, cloud ? XrayDeployment::Cloud : XrayDeployment::DataCenter));

    QString comment = QString("Robo Guide verdict: %1\nExpected: %2\nActual: %3\nSource: %4\nReason: %5\n"
                              "Reader: %6  Credential: %7  Mode: %8\nExecution: %9")
                          // Single multi-arg call: substituted text is never re-scanned for %N.
                          .arg(verdictName, QString::fromLatin1(toString(r.expected)),
                               QString::fromLatin1(toString(r.actual)), r.resultSource, r.reason, r.readerId,
                               r.credentialId, r.mode, r.executionId);
    if (r.resultSource == "OPERATOR_OBSERVED")
        comment += "\nNOTE: actual result was observed by the operator, not read from the platform.";

    QJsonObject test{{"testKey", r.jiraTestKey},
                     {"start", isoWithOffset(r.started)},
                     {"finish", isoWithOffset(r.finished)},
                     {"comment", comment},
                     {"status", status}};

    if (attachImages) {
        QJsonArray evidence;
        for (const auto& path : r.evidenceFiles) {
            if (!path.endsWith(".jpg", Qt::CaseInsensitive)) continue;
            QImage img(path);
            if (img.isNull()) continue;
            if (img.width() > 1280) img = img.scaledToWidth(1280, Qt::SmoothTransformation);
            QByteArray bytes;
            QBuffer buf(&bytes);
            buf.open(QIODevice::WriteOnly);
            img.save(&buf, "JPG", 80);
            evidence.append(QJsonObject{{"data", QString::fromLatin1(bytes.toBase64())},
                                        {"filename", QFileInfo(path).fileName()},
                                        {"contentType", "image/jpeg"}});
        }
        // Field name differs: Xray Cloud uses "evidence", Xray Server/DC uses "evidences".
        if (!evidence.isEmpty()) test[cloud ? "evidence" : "evidences"] = evidence;
    }

    QJsonObject root{{"tests", QJsonArray{test}}};
    if (!execKey.isEmpty()) {
        root["testExecutionKey"] = execKey;
    } else {
        QJsonObject info{{"summary", "Robo Guide execution " + r.executionId},
                         {"description", "Automated physical access-control test executed by Robo Guide."},
                         {"startDate", isoWithOffset(r.started)},
                         {"finishDate", isoWithOffset(r.finished)}};
        if (!cfg.jira.projectKey.isEmpty()) info["project"] = cfg.jira.projectKey;
        root["info"] = info;
    }
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}
