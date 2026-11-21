// Preserved from the original application: JSON task editor with a dry-run executor.
// Dry-run tasks never send commands to the robot.
#pragma once

#include <QWidget>

#include "AppContext.h"

class QPlainTextEdit;
class QTableWidget;
class QTimer;

class TaskDesignerPage : public QWidget {
    Q_OBJECT
public:
    explicit TaskDesignerPage(const AppContext& ctx, QWidget* parent = nullptr);

private:
    void loadTask();
    void saveTask();
    void runDry();
    void nextDry();
    void stopDry();

    AppContext ctx_;
    QPlainTextEdit* editor_ = nullptr;
    QTableWidget* table_ = nullptr;
    QTimer* timer_ = nullptr;
    bool running_ = false;
    int index_ = 0;
};
