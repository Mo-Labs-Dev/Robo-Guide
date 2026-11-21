#include "MainWindow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "ActivityLog.h"
#include "AppConfig.h"
#include "CameraManager.h"
#include "JiraXrayClient.h"
#include "PlatformClient.h"
#include "RosBridge.h"
#include "TestLibrary.h"
#include "TestRunner.h"
#include "UiHelpers.h"
#include "pages/AutonomousTestPage.h"
#include "pages/DashboardPage.h"
#include "pages/DiagnosticsPage.h"
#include "pages/JiraXrayPage.h"
#include "pages/PlatformPage.h"
#include "pages/RobotPage.h"
#include "pages/SettingsPage.h"
#include "pages/TaskDesignerPage.h"
#include "pages/TestCasePage.h"
#include "pages/VisionPage.h"

MainWindow::MainWindow(AppConfig* config, QWidget* parent) : QMainWindow(parent) {
    ctx_.config = config;
    ctx_.log = new ActivityLog(this);
    ctx_.ros = new RosBridge(this);
    ctx_.cameras = new CameraManager(config, this);
    ctx_.platform = createPlatformClient(config->platform.mode, this);
    ctx_.jira = new JiraXrayClient(config, this);
    ctx_.tests = new TestLibrary();
    ctx_.runner = new TestRunner(config, ctx_.cameras, ctx_.platform, this);

    connect(ctx_.ros, &RosBridge::logMessage, ctx_.log, &ActivityLog::info);
    connect(ctx_.cameras, &CameraManager::logMessage, ctx_.log, &ActivityLog::info);
    connect(ctx_.runner, &TestRunner::message, ctx_.log, [this](const QString& m) { ctx_.log->info("Test: " + m); });

    setWindowTitle("Robo Guide");
    resize(1380, 860);
    setStyleSheet(ui::styleSheet());

    auto* central = new QWidget;
    auto* outer = new QHBoxLayout(central);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* sidebar = new QWidget;
    sidebar->setFixedWidth(215);
    sidebar->setStyleSheet("background:#0b1320;");
    auto* nav = new QVBoxLayout(sidebar);
    nav->setContentsMargins(14, 20, 14, 14);
    auto* brand = new QLabel("Robo Guide");
    brand->setObjectName("brand");
    nav->addWidget(brand);
    nav->addSpacing(18);

    pages_ = new QStackedWidget;
    auto* testCases = new TestCasePage(ctx_);
    auto* autonomous = new AutonomousTestPage(ctx_);
    struct Entry {
        QString name;
        QWidget* page;
    };
    const QVector<Entry> entries = {
        {"Dashboard", new DashboardPage(ctx_)},
        {"Robot Control", new RobotPage(ctx_)},
        {"Vision Studio", new VisionPage(ctx_)},
        {"Test Case Manager", testCases},
        {"Task Designer", new TaskDesignerPage(ctx_)},
        {"Autonomous Testing", autonomous},
        {"Platform Integration", new PlatformPage(ctx_)},
        {"Jira / Xray", new JiraXrayPage(ctx_)},
        {"Diagnostics", new DiagnosticsPage(ctx_)},
        {"Settings", new SettingsPage(ctx_)},
    };
    for (int i = 0; i < entries.size(); ++i) {
        pages_->addWidget(entries[i].page);
        auto* b = new QPushButton(entries[i].name);
        b->setObjectName("nav");
        b->setCheckable(true);
        b->setMinimumHeight(40);
        nav->addWidget(b);
        nav_.push_back(b);
        connect(b, &QPushButton::clicked, this, [this, i]() { showPage(i); });
    }
    nav->addStretch();
    footer_ = new QLabel;
    footer_->setWordWrap(true);
    nav->addWidget(footer_);

    outer->addWidget(sidebar);
    outer->addWidget(pages_, 1);
    setCentralWidget(central);

    const int autonomousIndex = 5;
    connect(testCases, &TestCasePage::testSelected, this,
            [this, autonomous, autonomousIndex](const QString& defId, const QString& jiraKey) {
                autonomous->selectTest(defId, jiraKey);
                showPage(autonomousIndex);
            });

    auto* footerTimer = new QTimer(this);
    connect(footerTimer, &QTimer::timeout, this, [this]() {
        const bool locked = !ctx_.motionGate().allowed;
        footer_->setText(QString("Motion: %1\nCameras: %2/%3 live\nPlatform: %4")
                             .arg(locked ? "LOCKED" : "ARMED")
                             .arg(ctx_.cameras->connectedCount())
                             .arg(ctx_.cameras->count())
                             .arg(ctx_.platform->name()));
        footer_->setStyleSheet(locked ? "color:#91a5bf;" : "color:#ff7b72;font-weight:bold;");
    });
    footerTimer->start(1000);

    showPage(0);
    ctx_.log->info("Robo Guide started - physical motion locked");
    ctx_.log->info("Configuration: " + config->loadedFrom);
}

MainWindow::~MainWindow() {
    if (ctx_.runner->isRunning()) ctx_.runner->abort("Application closed");
    ctx_.cameras->stopAll();
    // Destroy the pages before the services they reference (services are QObject children
    // created earlier and would otherwise be deleted first).
    delete takeCentralWidget();
    delete ctx_.tests;
    ctx_.tests = nullptr;
}

void MainWindow::showPage(int index) {
    pages_->setCurrentIndex(index);
    for (int i = 0; i < nav_.size(); ++i) nav_[i]->setChecked(i == index);
}
