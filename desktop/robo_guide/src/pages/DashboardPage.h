#pragma once

#include <QWidget>

#include "AppContext.h"

class QLabel;

class DashboardPage : public QWidget {
    Q_OBJECT
public:
    explicit DashboardPage(const AppContext& ctx, QWidget* parent = nullptr);

private:
    void refresh();
    AppContext ctx_;
    QLabel *robot_ = nullptr, *cameras_ = nullptr, *platform_ = nullptr, *jira_ = nullptr, *motion_ = nullptr,
           *lastRun_ = nullptr;
};
