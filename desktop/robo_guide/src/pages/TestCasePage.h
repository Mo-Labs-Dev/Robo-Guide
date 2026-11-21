#pragma once

#include <QVector>
#include <QWidget>

#include "AppContext.h"
#include "JiraXrayClient.h"

class QLabel;
class QTableWidget;

class TestCasePage : public QWidget {
    Q_OBJECT
public:
    explicit TestCasePage(const AppContext& ctx, QWidget* parent = nullptr);

signals:
    void testSelected(const QString& definitionId, const QString& jiraKey);

private:
    void reloadDefinitions();
    void fetchJira();
    void showJira(const QVector<JiraTest>& tests, const QString& error);
    void useSelectedDefinition();
    void useSelectedJira();

    AppContext ctx_;
    QTableWidget* defs_ = nullptr;
    QTableWidget* jira_ = nullptr;
    QLabel* defsInfo_ = nullptr;
    QLabel* jiraInfo_ = nullptr;
};
