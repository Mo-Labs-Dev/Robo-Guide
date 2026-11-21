#include "JiraXrayPage.h"

#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>

#include "AppConfig.h"
#include "JiraXrayClient.h"
#include "UiHelpers.h"

static QString yesNo(bool b) { return b ? "<span style='color:#56d364'>set</span>" : "<span style='color:#ff7b72'>not set</span>"; }

JiraXrayPage::JiraXrayPage(const AppContext& ctx, QWidget* parent) : QWidget(parent), ctx_(ctx) {
    auto* l = ui::makePage(this, "Jira / Xray");
    l->addWidget(ui::note(
        "Tests are fetched read-only via JQL. Results are published only on explicit request, using the Xray JSON "
        "import format. Credentials are read from environment variables and never stored by Robo Guide."));

    auto* g = new QGroupBox("Connection");
    auto* gl = new QVBoxLayout(g);
    summary_ = new QLabel;
    summary_->setTextFormat(Qt::RichText);
    summary_->setWordWrap(true);
    gl->addWidget(summary_);
    auto* bar = new QHBoxLayout;
    auto* test = new QPushButton("Test connection (fetch tests)");
    auto* import = new QPushButton("Import an xray_import.json file...");
    bar->addWidget(test);
    bar->addWidget(import);
    bar->addStretch();
    gl->addLayout(bar);
    l->addWidget(g);

    log_ = new QPlainTextEdit;
    log_->setReadOnly(true);
    l->addWidget(log_, 1);

    connect(test, &QPushButton::clicked, this, [this]() {
        log_->appendPlainText("Fetching tests...");
        ctx_.jira->searchTests();
    });
    connect(import, &QPushButton::clicked, this, &JiraXrayPage::importFile);
    connect(ctx_.jira, &JiraXrayClient::testsReady, this, [this](const QVector<JiraTest>& t, const QString& err) {
        log_->appendPlainText(err.isEmpty() ? QString("OK - %1 test(s) found").arg(t.size()) : "ERROR - " + err);
    });
    connect(ctx_.jira, &JiraXrayClient::importFinished, this, [this](bool ok, const QString& msg) {
        log_->appendPlainText(QString("%1 import: %2").arg(QString(ok ? "OK" : "ERROR"), msg));
    });
    refresh();
}

void JiraXrayPage::refresh() {
    const auto& j = ctx_.config->jira;
    QString why, xwhy;
    const bool jok = ctx_.jira->jiraConfigured(&why);
    const bool xok = ctx_.jira->xrayConfigured(&xwhy);
    summary_->setText(
        QString("<b>Deployment:</b> %1<br><b>Jira URL:</b> %2<br><b>Xray URL:</b> %3<br><b>Project:</b> %4<br>"
                "<b>Test JQL:</b> %5<br><br>RG_JIRA_USER: %6 &nbsp; RG_JIRA_TOKEN: %7 &nbsp; RG_XRAY_CLIENT_ID: %8 &nbsp; "
                "RG_XRAY_CLIENT_SECRET: %9<br><br><b>Jira:</b> ")
            .arg(j.deployment.toHtmlEscaped(), j.baseUrl.toHtmlEscaped(),
                 ctx_.jira->isCloud() ? j.xrayBaseUrl.toHtmlEscaped() : QString("(uses Jira URL)"),
                 j.projectKey.toHtmlEscaped(), j.testJql.isEmpty() ? QString("(default)") : j.testJql.toHtmlEscaped(),
                 yesNo(AppConfig::hasSecret("RG_JIRA_USER")), yesNo(AppConfig::hasSecret("RG_JIRA_TOKEN")),
                 yesNo(AppConfig::hasSecret("RG_XRAY_CLIENT_ID")), yesNo(AppConfig::hasSecret("RG_XRAY_CLIENT_SECRET"))) +
        (jok ? QString("ready") : why.toHtmlEscaped()) + "<br><b>Xray:</b> " + (xok ? QString("ready") : xwhy.toHtmlEscaped()));
}

void JiraXrayPage::importFile() {
    const QString file = QFileDialog::getOpenFileName(this, "Xray import file", ctx_.config->resolvedEvidenceDir(), "JSON (*.json)");
    if (file.isEmpty()) return;
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) return;
    const QByteArray data = f.readAll();
    if (!QJsonDocument::fromJson(data).isObject()) {
        QMessageBox::warning(this, "Invalid file", "Not a JSON object.");
        return;
    }
    if (data.contains("\"SIMULATED_PLATFORM\"") || data.contains("Source: SIMULATED_PLATFORM")) {
        QMessageBox::warning(this, "Refused", "This file contains simulated results and will not be published.");
        return;
    }
    if (QMessageBox::question(this, "Publish", "Publish " + file + " to Xray?") != QMessageBox::Yes) return;
    log_->appendPlainText("Publishing " + file);
    ctx_.jira->importExecution(data);
}
