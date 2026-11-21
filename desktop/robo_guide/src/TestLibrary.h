// Loads approved test definitions (config/test_definitions/*.json) and the explicit
// Jira-key -> definition bindings (config/test_bindings.json).
#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include "robo_guide/core/test_definition.hpp"

struct LoadedDefinition {
    QString file;
    rg::core::TestDefinition def;
    QStringList problems;  // parse + validation problems; empty = valid
    bool valid() const { return problems.isEmpty(); }
};

class TestLibrary {
public:
    void load(const QString& definitionsDir, const QString& bindingsFile);

    const QVector<LoadedDefinition>& definitions() const { return defs_; }
    const LoadedDefinition* find(const QString& id) const;
    // Definition id bound to a Jira test key, or empty if the test is unsupported.
    QString bindingFor(const QString& jiraKey) const { return bindings_.value(jiraKey); }
    QStringList loadErrors() const { return errors_; }

    static bool parse(const QByteArray& json, rg::core::TestDefinition& out, QStringList& problems);

private:
    QVector<LoadedDefinition> defs_;
    QMap<QString, QString> bindings_;
    QStringList errors_;
};
