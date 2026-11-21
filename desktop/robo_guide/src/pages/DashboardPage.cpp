#include "DashboardPage.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QTimer>

#include "CameraManager.h"
#include "ExecutionRecord.h"
#include "JiraXrayClient.h"
#include "PlatformClient.h"
#include "RosBridge.h"
#include "TestRunner.h"
#include "UiHelpers.h"

static QGroupBox* tile(const QString& title, QLabel*& value) {
    auto* g = new QGroupBox(title);
    auto* l = new QVBoxLayout(g);
    value = new QLabel("-");
    value->setWordWrap(true);
    l->addWidget(value);
    l->addStretch();
    g->setMinimumHeight(110);
    return g;
}

DashboardPage::DashboardPage(const AppContext& ctx, QWidget* parent) : QWidget(parent), ctx_(ctx) {
    auto* l = ui::makePage(this, "Dashboard");
    l->addWidget(ui::note("Waveshare RoArm-M2-S  -  ROS 2 Jazzy  -  C++17 / Qt 6"));
    auto* grid = new QGridLayout;
    grid->addWidget(tile("Robot", robot_), 0, 0);
    grid->addWidget(tile("Motion safety gate", motion_), 0, 1);
    grid->addWidget(tile("Cameras", cameras_), 0, 2);
    grid->addWidget(tile("Access-management platform", platform_), 1, 0);
    grid->addWidget(tile("Jira / Xray", jira_), 1, 1);
    grid->addWidget(tile("Last test execution", lastRun_), 1, 2);
    l->addLayout(grid);
    l->addWidget(ui::note(
        "Physical robot motion is locked. Physical test steps currently run in operator-assisted mode: "
        "Robo Guide prompts, times, correlates, judges and records evidence; a person moves the card."));
    l->addStretch();

    connect(ctx_.runner, &TestRunner::finished, this, [this](const ExecutionRecord& r) {
        const QString v = QString::fromLatin1(rg::core::toString(r.verdict));
        lastRun_->setText(QString("<b style='color:%1'>%2</b><br>%3<br>%4")
                              .arg(ui::verdictColor(v), v, r.testId.toHtmlEscaped(), r.reason.toHtmlEscaped()));
    });
    auto* t = new QTimer(this);
    connect(t, &QTimer::timeout, this, &DashboardPage::refresh);
    t->start(1000);
    refresh();
}

void DashboardPage::refresh() {
    robot_->setText(ctx_.ros->statusText());
    const auto gate = ctx_.motionGate();
    motion_->setText(gate.allowed ? "<b style='color:#ff7b72'>ARMED</b>"
                                  : QString("<b>LOCKED</b><br>%1 precondition(s) unmet").arg(gate.unmet.size()));
    int enabled = 0;
    for (int i = 0; i < ctx_.cameras->count(); ++i)
        if (ctx_.cameras->isEnabled(i)) ++enabled;
    cameras_->setText(QString("%1 live / %2 enabled / %3 configured slots")
                          .arg(ctx_.cameras->connectedCount())
                          .arg(enabled)
                          .arg(ctx_.cameras->count()));
    platform_->setText(ctx_.platform->name() + (ctx_.platform->isAvailable() ? "" : "<br>Read-only interface pending approval"));
    QString why;
    jira_->setText(ctx_.jira->jiraConfigured(&why) ? QString("Configured (%1)").arg(ctx_.jira->isCloud() ? "Cloud" : "Data Center")
                                                   : "Not configured: " + why);
    if (lastRun_->text() == "-") lastRun_->setText("No execution in this session");
}
