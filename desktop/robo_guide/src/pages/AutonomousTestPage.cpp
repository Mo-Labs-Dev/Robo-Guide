#include "AutonomousTestPage.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>

#include "ActivityLog.h"
#include "AppConfig.h"
#include "JiraXrayClient.h"
#include "TestLibrary.h"
#include "TestRunner.h"
#include "UiHelpers.h"

using namespace rg::core;

static const Outcome kObservable[] = {Outcome::AccessGranted, Outcome::AccessDenied, Outcome::CredentialRecognized,
                                      Outcome::CredentialUnknown, Outcome::CommunicationError, Outcome::NoEvent};

AutonomousTestPage::AutonomousTestPage(const AppContext& ctx, QWidget* parent) : QWidget(parent), ctx_(ctx) {
    auto* l = ui::makePage(this, "Autonomous Testing");
    l->addWidget(ui::note(
        "Pipeline: approved test definition -> execution plan -> physical steps -> platform event correlation -> "
        "verdict -> evidence -> Xray. While robot motion is locked, physical steps are operator-assisted."));

    auto* top = new QHBoxLayout;
    l->addLayout(top, 1);
    auto* left = new QVBoxLayout;
    auto* right = new QVBoxLayout;
    top->addLayout(left, 3);
    top->addLayout(right, 2);

    auto* sg = new QGroupBox("Test");
    auto* sf = new QFormLayout(sg);
    selected_ = new QLabel("No test selected - choose one in Test Case Manager");
    selected_->setWordWrap(true);
    mode_ = new QComboBox;
    mode_->addItem("Operator-assisted (robot motion locked)");
    mode_->addItem("Robot");
    execKey_ = new QLineEdit;
    execKey_->setPlaceholderText("Optional existing Xray Test Execution key, e.g. PROJ-123");
    sf->addRow("Selected", selected_);
    sf->addRow("Presentation", mode_);
    sf->addRow("Xray execution", execKey_);
    left->addWidget(sg);

    auto* pg = new QGroupBox("Execution plan");
    auto* pl = new QVBoxLayout(pg);
    planInfo_ = ui::note("");
    pl->addWidget(planInfo_);
    plan_ = new QTableWidget(0, 4);
    plan_->setHorizontalHeaderLabels({"#", "Action", "Execution", "Status"});
    plan_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    plan_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    plan_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pl->addWidget(plan_, 1);
    auto* rb = new QHBoxLayout;
    run_ = new QPushButton("Run test");
    run_->setObjectName("primary");
    abort_ = new QPushButton("Abort");
    abort_->setEnabled(false);
    rb->addWidget(run_);
    rb->addWidget(abort_);
    rb->addStretch();
    pl->addLayout(rb);
    left->addWidget(pg, 1);

    auto* og = new QGroupBox("Operator");
    promptBox_ = og;
    auto* ol = new QVBoxLayout(og);
    promptText_ = new QLabel("No action required.");
    promptText_->setWordWrap(true);
    promptText_->setStyleSheet("font-size:15px;");
    ol->addWidget(promptText_);
    confirm_ = new QPushButton("Confirm step done");
    confirm_->setObjectName("primary");
    confirm_->setVisible(false);
    ol->addWidget(confirm_);
    auto* obsRow = new QHBoxLayout;
    observed_ = new QComboBox;
    for (auto o : kObservable) observed_->addItem(QString::fromLatin1(toString(o)), static_cast<int>(o));
    record_ = new QPushButton("Record observed outcome");
    obsRow->addWidget(observed_, 1);
    obsRow->addWidget(record_);
    ol->addLayout(obsRow);
    observed_->setVisible(false);
    record_->setVisible(false);
    right->addWidget(og);

    auto* vg = new QGroupBox("Result");
    auto* vl = new QVBoxLayout(vg);
    verdict_ = new QLabel("-");
    verdict_->setStyleSheet("font-size:30px;font-weight:800;");
    reason_ = new QLabel;
    reason_->setWordWrap(true);
    reason_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    vl->addWidget(verdict_);
    vl->addWidget(reason_);
    auto* eb = new QHBoxLayout;
    openEvidence_ = new QPushButton("Open evidence folder");
    publish_ = new QPushButton("Publish to Xray");
    openEvidence_->setEnabled(false);
    publish_->setEnabled(false);
    eb->addWidget(openEvidence_);
    eb->addWidget(publish_);
    vl->addLayout(eb);
    right->addWidget(vg);

    log_ = new QPlainTextEdit;
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(500);
    right->addWidget(log_, 1);

    connect(mode_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { refreshPlan(); });
    connect(run_, &QPushButton::clicked, this, &AutonomousTestPage::run);
    connect(abort_, &QPushButton::clicked, this, [this]() { ctx_.runner->abort("Operator abort"); });
    connect(confirm_, &QPushButton::clicked, ctx_.runner, &TestRunner::confirmStep);
    connect(record_, &QPushButton::clicked, this, [this]() {
        ctx_.runner->submitObservation(static_cast<Outcome>(observed_->currentData().toInt()));
    });
    connect(openEvidence_, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(ctx_.runner->lastRecord().evidenceDir));
    });
    connect(publish_, &QPushButton::clicked, this, &AutonomousTestPage::publish);

    connect(ctx_.runner, &TestRunner::planReady, this, &AutonomousTestPage::showPlan);
    connect(ctx_.runner, &TestRunner::stepChanged, this, [this](int i, const QString& s) {
        if (i >= 0 && i < plan_->rowCount()) plan_->setItem(i, 3, new QTableWidgetItem(s));
    });
    connect(ctx_.runner, &TestRunner::prompt, this, &AutonomousTestPage::onPrompt);
    connect(ctx_.runner, &TestRunner::message, log_, &QPlainTextEdit::appendPlainText);
    connect(ctx_.runner, &TestRunner::finished, this, &AutonomousTestPage::onFinished);
    connect(ctx_.jira, &JiraXrayClient::importFinished, this, [this](bool ok, const QString& msg) {
        log_->appendPlainText((ok ? "Xray: " : "Xray ERROR: ") + msg);
        if (ok) ctx_.log->info("Xray: " + msg);
        else ctx_.log->warn("Xray: " + msg);
        publish_->setEnabled(haveRecord_);
    });
}

PresentationMode AutonomousTestPage::mode() const {
    return mode_->currentIndex() == 1 ? PresentationMode::Robot : PresentationMode::OperatorAssisted;
}

void AutonomousTestPage::selectTest(const QString& definitionId, const QString& jiraKey) {
    if (ctx_.runner->isRunning()) {
        QMessageBox::information(this, "Test running", "Finish or abort the current test first.");
        return;
    }
    defId_ = definitionId;
    jiraKey_ = jiraKey;
    const auto* ld = ctx_.tests->find(definitionId);
    if (!ld) {
        selected_->setText("Definition '" + definitionId + "' not found");
    } else {
        selected_->setText(QString("<b>%1</b> - %2<br>Reader %3, credential %4, expected %5%6")
                               .arg(QString::fromStdString(ld->def.id).toHtmlEscaped(),
                                    QString::fromStdString(ld->def.title).toHtmlEscaped(),
                                    QString::fromStdString(ld->def.readerId).toHtmlEscaped(),
                                    QString::fromStdString(ld->def.credentialId).toHtmlEscaped(),
                                    QString::fromLatin1(toString(ld->def.expected)),
                                    jiraKey.isEmpty() ? QString() : "<br>Jira test " + jiraKey.toHtmlEscaped()));
    }
    refreshPlan();
}

void AutonomousTestPage::refreshPlan() {
    if (ctx_.runner->isRunning()) return;
    const auto* ld = ctx_.tests->find(defId_);
    if (!ld) {
        plan_->setRowCount(0);
        planInfo_->setText(defId_.isEmpty() ? QString() : "Definition not found");
        return;
    }
    showPlan(ctx_.runner->preview(ld->def, mode(), ctx_.motionGate().allowed));
}

void AutonomousTestPage::showPlan(const ExecutionPlan& p) {
    plan_->setRowCount(static_cast<int>(p.steps.size()));
    for (int i = 0; i < static_cast<int>(p.steps.size()); ++i) {
        const auto& s = p.steps[static_cast<size_t>(i)];
        QString how;
        switch (s.status) {
            case StepStatus::Ready: how = isRobotAction(s.step.type) ? "Robot" : "Automatic"; break;
            case StepStatus::OperatorAction: how = "Operator"; break;
            case StepStatus::Blocked: how = "BLOCKED"; break;
        }
        if (!s.reason.empty()) how += " - " + QString::fromStdString(s.reason);
        plan_->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        QString action = QString::fromLatin1(toString(s.step.type));
        if (!s.step.label.empty()) action += ": " + QString::fromStdString(s.step.label);
        plan_->setItem(i, 1, new QTableWidgetItem(action));
        plan_->setItem(i, 2, new QTableWidgetItem(how));
        plan_->setItem(i, 3, new QTableWidgetItem("PENDING"));
    }
    QStringList problems;
    for (const auto& pr : p.problems) problems << QString::fromStdString(pr);
    planInfo_->setText(p.runnable ? "Plan is runnable."
                                  : "Not runnable - the run will be recorded as BLOCKED:\n" + problems.join("\n"));
}

void AutonomousTestPage::run() {
    const auto* ld = ctx_.tests->find(defId_);
    if (!ld) {
        QMessageBox::information(this, "No test", "Select a test in Test Case Manager first.");
        return;
    }
    if (!ld->valid()) {
        QMessageBox::warning(this, "Invalid definition", ld->problems.join("\n"));
        return;
    }
    log_->clear();
    verdict_->setText("RUNNING");
    verdict_->setStyleSheet("font-size:30px;font-weight:800;color:#70b9ff;");
    reason_->clear();
    haveRecord_ = false;
    publish_->setEnabled(false);
    openEvidence_->setEnabled(false);
    QString err;
    if (!ctx_.runner->start(ld->def, mode(), ctx_.motionGate().allowed, jiraKey_, &err)) {
        QMessageBox::warning(this, "Cannot start", err);
        return;
    }
    run_->setEnabled(false);
    abort_->setEnabled(true);
    mode_->setEnabled(false);
}

void AutonomousTestPage::onPrompt(int kind, const QString& text) {
    confirm_->setVisible(kind == TestRunner::PromptConfirm);
    observed_->setVisible(kind == TestRunner::PromptObserve);
    record_->setVisible(kind == TestRunner::PromptObserve);
    if (kind == TestRunner::PromptObserve) {
        // Default to NO_EVENT so an inattentive click never fabricates the expected outcome.
        observed_->setCurrentIndex(observed_->count() - 1);
    }
    promptText_->setText(kind == TestRunner::PromptNone ? "No action required." : text);
    promptBox_->setStyleSheet(kind == TestRunner::PromptNone ? QString() : "QGroupBox{border:2px solid #e3b341;}");
}

void AutonomousTestPage::onFinished(const ExecutionRecord& r) {
    const QString v = QString::fromLatin1(toString(r.verdict));
    verdict_->setText(v + (r.simulated() ? "  (SIMULATED)" : ""));
    verdict_->setStyleSheet(QString("font-size:30px;font-weight:800;color:%1;").arg(ui::verdictColor(v)));
    reason_->setText(QString("Expected %1, actual %2 (source: %3)\n%4\nEvidence: %5")
                         .arg(QString::fromLatin1(toString(r.expected)), QString::fromLatin1(toString(r.actual)),
                              r.resultSource, r.reason, r.evidenceDir));
    haveRecord_ = true;
    run_->setEnabled(true);
    abort_->setEnabled(false);
    mode_->setEnabled(true);
    openEvidence_->setEnabled(true);
    publish_->setEnabled(true);
    onPrompt(TestRunner::PromptNone, QString());
    ctx_.log->info(QString("Test %1 finished: %2 - %3").arg(r.testId, v, r.reason));
}

void AutonomousTestPage::publish() {
    const ExecutionRecord& r = ctx_.runner->lastRecord();
    if (!haveRecord_) return;
    if (r.simulated()) {
        QMessageBox::warning(this, "Not published", "Results produced with the SIMULATED platform are never published to Xray.");
        return;
    }
    if (r.jiraTestKey.isEmpty()) {
        QMessageBox::warning(this, "Not published", "This run is not linked to a Jira test. Start it from a bound Jira test.");
        return;
    }
    const QByteArray payload = Evidence::buildXrayJson(r, *ctx_.config, execKey_->text().trimmed(), true);
    QFile f(QDir(r.evidenceDir).filePath("xray_import.json"));
    if (f.open(QIODevice::WriteOnly)) f.write(payload);
    const auto answer = QMessageBox::question(
        this, "Publish to Xray",
        QString("Publish %1 for %2 to Xray%3?\nThe payload was saved as xray_import.json in the evidence folder.")
            .arg(QString::fromLatin1(toString(r.verdict)), r.jiraTestKey,
                 execKey_->text().trimmed().isEmpty() ? " (creates a new Test Execution)" : " execution " + execKey_->text().trimmed()));
    if (answer != QMessageBox::Yes) return;
    publish_->setEnabled(false);
    log_->appendPlainText("Publishing to Xray...");
    ctx_.jira->importExecution(payload);
}
