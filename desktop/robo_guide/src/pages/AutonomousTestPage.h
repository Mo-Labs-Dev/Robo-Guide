#pragma once

#include <QWidget>

#include "AppContext.h"
#include "ExecutionRecord.h"
#include "robo_guide/core/test_definition.hpp"

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QWidget;

class AutonomousTestPage : public QWidget {
    Q_OBJECT
public:
    explicit AutonomousTestPage(const AppContext& ctx, QWidget* parent = nullptr);
    void selectTest(const QString& definitionId, const QString& jiraKey);

private:
    void refreshPlan();
    void showPlan(const rg::core::ExecutionPlan& plan);
    void run();
    void onPrompt(int kind, const QString& text);
    void onFinished(const ExecutionRecord& r);
    void publish();
    rg::core::PresentationMode mode() const;

    AppContext ctx_;
    QString defId_;
    QString jiraKey_;
    bool haveRecord_ = false;

    QLabel* selected_ = nullptr;
    QComboBox* mode_ = nullptr;
    QLineEdit* execKey_ = nullptr;
    QTableWidget* plan_ = nullptr;
    QLabel* planInfo_ = nullptr;
    QPushButton *run_ = nullptr, *abort_ = nullptr;

    QWidget* promptBox_ = nullptr;
    QLabel* promptText_ = nullptr;
    QPushButton* confirm_ = nullptr;
    QComboBox* observed_ = nullptr;
    QPushButton* record_ = nullptr;

    QLabel* verdict_ = nullptr;
    QLabel* reason_ = nullptr;
    QPushButton *openEvidence_ = nullptr, *publish_ = nullptr;
    QPlainTextEdit* log_ = nullptr;
};
