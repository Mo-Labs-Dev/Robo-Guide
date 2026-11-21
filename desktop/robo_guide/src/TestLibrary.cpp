#include "TestLibrary.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace rg::core;

bool TestLibrary::parse(const QByteArray& json, TestDefinition& d, QStringList& problems) {
    QJsonParseError pe;
    const auto doc = QJsonDocument::fromJson(json, &pe);
    if (!doc.isObject()) {
        problems << "JSON error: " + pe.errorString();
        return false;
    }
    const auto o = doc.object();
    d.id = o.value("id").toString().toStdString();
    d.title = o.value("title").toString().toStdString();
    d.credentialId = o.value("credentialId").toString().toStdString();
    d.readerId = o.value("readerId").toString().toStdString();
    d.eventLatencyMs = o.value("eventLatencyMs").toInt(3000);
    d.clockSkewMs = o.value("clockSkewMs").toInt(1000);
    const auto expected = parseOutcome(o.value("expected").toString().toStdString());
    if (!expected) {
        problems << "Unknown expected outcome: " + o.value("expected").toString();
    } else {
        d.expected = *expected;
    }
    d.steps.clear();
    const auto steps = o.value("steps").toArray();
    for (int i = 0; i < steps.size(); ++i) {
        const auto s = steps.at(i).toObject();
        TestStep step;
        const QString type = s.value("type").toString();
        if (!parseActionType(type.toStdString(), step.type)) {
            problems << QString("Step %1: '%2' is not an approved action").arg(i + 1).arg(type);
            continue;
        }
        step.label = s.value("label").toString().toStdString();
        step.durationMs = s.value("durationMs").toInt(0);
        d.steps.push_back(step);
    }
    for (const auto& p : validate(d)) problems << QString::fromStdString(p);
    return problems.isEmpty();
}

void TestLibrary::load(const QString& dir, const QString& bindingsFile) {
    defs_.clear();
    bindings_.clear();
    errors_.clear();

    const QDir d(dir);
    if (!d.exists()) errors_ << "Definitions folder not found: " + dir;
    for (const auto& fi : d.entryInfoList({"*.json"}, QDir::Files, QDir::Name)) {
        LoadedDefinition ld;
        ld.file = fi.absoluteFilePath();
        QFile f(ld.file);
        if (!f.open(QIODevice::ReadOnly)) {
            ld.problems << "Cannot read file";
        } else {
            parse(f.readAll(), ld.def, ld.problems);
        }
        if (ld.def.id.empty()) ld.def.id = fi.completeBaseName().toStdString();
        if (find(QString::fromStdString(ld.def.id))) ld.problems << "Duplicate definition id";
        defs_.push_back(ld);
    }

    if (!QFileInfo::exists(bindingsFile)) return;  // no bindings yet is fine
    QFile bf(bindingsFile);
    if (!bf.open(QIODevice::ReadOnly)) {
        errors_ << "Cannot read bindings: " + bindingsFile;
        return;
    }
    const auto obj = QJsonDocument::fromJson(bf.readAll()).object().value("bindings").toObject();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const QString defId = it.value().toString();
        if (!find(defId)) errors_ << QString("Binding %1 -> %2: definition not found").arg(it.key(), defId);
        bindings_.insert(it.key(), defId);
    }
}

const LoadedDefinition* TestLibrary::find(const QString& id) const {
    for (const auto& d : defs_)
        if (QString::fromStdString(d.def.id) == id) return &d;
    return nullptr;
}
