#include "TaskDesignerPage.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>

#include "ActivityLog.h"
#include "AppConfig.h"
#include "UiHelpers.h"

TaskDesignerPage::TaskDesignerPage(const AppContext& ctx, QWidget* parent) : QWidget(parent), ctx_(ctx) {
    auto* l = ui::makePage(this, "Task Designer");
    l->addWidget(ui::note("Design reusable workflow JSON and dry-run it. Dry runs never send robot commands. "
                          "Executable physical tests are defined as approved test definitions "
                          "(config/test_definitions) and run from Autonomous Testing."));
    auto* g = new QGroupBox("Task JSON");
    auto* gl = new QVBoxLayout(g);
    editor_ = new QPlainTextEdit;
    editor_->setPlainText("{\n  \"name\": \"Example inspection\",\n  \"steps\": [\n"
                          "    {\"type\": \"note\", \"label\": \"Check fixture\"},\n"
                          "    {\"type\": \"wait\", \"ms\": 500},\n"
                          "    {\"type\": \"note\", \"label\": \"Record inspection result\"}\n  ]\n}");
    gl->addWidget(editor_);
    auto* bar = new QHBoxLayout;
    auto* load = new QPushButton("Load JSON");
    auto* save = new QPushButton("Save JSON");
    auto* run = new QPushButton("Run dry-run");
    auto* stop = new QPushButton("Stop");
    bar->addWidget(load);
    bar->addWidget(save);
    bar->addWidget(run);
    bar->addWidget(stop);
    gl->addLayout(bar);
    l->addWidget(g, 2);

    table_ = new QTableWidget(0, 3);
    table_->setHorizontalHeaderLabels({"Step", "Type", "Result"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    l->addWidget(table_, 1);

    timer_ = new QTimer(this);
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, &TaskDesignerPage::nextDry);
    connect(load, &QPushButton::clicked, this, &TaskDesignerPage::loadTask);
    connect(save, &QPushButton::clicked, this, &TaskDesignerPage::saveTask);
    connect(run, &QPushButton::clicked, this, &TaskDesignerPage::runDry);
    connect(stop, &QPushButton::clicked, this, &TaskDesignerPage::stopDry);
}

void TaskDesignerPage::loadTask() {
    const QString file = QFileDialog::getOpenFileName(this, "Load task", AppConfig::repoRoot() + "/config/tasks", "JSON (*.json)");
    if (file.isEmpty()) return;
    QFile f(file);
    if (f.open(QIODevice::ReadOnly)) editor_->setPlainText(QString::fromUtf8(f.readAll()));
}

void TaskDesignerPage::saveTask() {
    const auto doc = QJsonDocument::fromJson(editor_->toPlainText().toUtf8());
    if (!doc.isObject() || !doc.object().value("steps").isArray()) {
        QMessageBox::warning(this, "Invalid task", "Task must be a JSON object with a steps array.");
        return;
    }
    const QString file = QFileDialog::getSaveFileName(this, "Save task", QDir::homePath() + "/task.json", "JSON (*.json)");
    if (file.isEmpty()) return;
    QFile f(file);
    if (f.open(QIODevice::WriteOnly)) f.write(doc.toJson(QJsonDocument::Indented));
}

void TaskDesignerPage::runDry() {
    if (running_) return;
    const auto doc = QJsonDocument::fromJson(editor_->toPlainText().toUtf8());
    if (!doc.isObject() || !doc.object().value("steps").isArray()) {
        QMessageBox::warning(this, "Invalid task", "Expected JSON with a steps array.");
        return;
    }
    const auto steps = doc.object().value("steps").toArray();
    for (const auto& v : steps) {
        if (!v.isObject() || !QStringList({"note", "wait"}).contains(v.toObject().value("type").toString())) {
            QMessageBox::warning(this, "Unsupported step", "Dry-run supports only note and wait steps.");
            return;
        }
    }
    table_->setRowCount(steps.size());
    for (int i = 0; i < steps.size(); ++i) {
        table_->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        table_->setItem(i, 1, new QTableWidgetItem(steps[i].toObject().value("type").toString()));
        table_->setItem(i, 2, new QTableWidgetItem("PENDING"));
    }
    running_ = true;
    index_ = 0;
    ctx_.log->info("Dry-run started (no hardware commands)");
    nextDry();
}

void TaskDesignerPage::nextDry() {
    if (!running_) return;
    const auto steps = QJsonDocument::fromJson(editor_->toPlainText().toUtf8()).object().value("steps").toArray();
    if (index_ >= steps.size() || index_ >= table_->rowCount()) {
        running_ = false;
        ctx_.log->info("Dry-run completed");
        return;
    }
    const auto step = steps[index_].toObject();
    const QString type = step.value("type").toString();
    table_->item(index_, 2)->setText("PASS (SIMULATED)");
    ctx_.log->info(QString("Dry-run step %1: %2").arg(index_ + 1).arg(type));
    ++index_;
    timer_->start(type == "wait" ? qBound(0, step.value("ms").toInt(500), 5000) : 350);
}

void TaskDesignerPage::stopDry() {
    timer_->stop();
    if (running_) {
        running_ = false;
        ctx_.log->info("Dry-run stopped");
    }
}
