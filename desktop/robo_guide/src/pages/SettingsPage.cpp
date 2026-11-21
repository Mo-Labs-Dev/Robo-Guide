#include "SettingsPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>

#include "ActivityLog.h"
#include "AppConfig.h"
#include "UiHelpers.h"

namespace {
enum CamCol { ColEnabled, ColName, ColRole, ColHost, ColPort, ColStream, ColUrl, ColCount };

const char* kSecretNames[] = {"RG_CAMERA_USER", "RG_CAMERA_PASSWORD", "RG_JIRA_USER", "RG_JIRA_TOKEN",
                              "RG_XRAY_CLIENT_ID", "RG_XRAY_CLIENT_SECRET"};
}  // namespace

SettingsPage::SettingsPage(const AppContext& ctx, QWidget* parent) : QWidget(parent), ctx_(ctx) {
    auto* outer = ui::makePage(this, "Settings");
    outer->addWidget(ui::note("Non-secret settings are saved to " + AppConfig::userConfigPath() +
                              ". Secrets are read only from environment variables (config/.env, loaded by "
                              "scripts/start_gui.sh). Restart Robo Guide after saving."));

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* body = new QWidget;
    auto* l = new QVBoxLayout(body);
    scroll->setWidget(body);
    outer->addWidget(scroll, 1);

    auto* robotGroup = new QGroupBox("Robot");
    auto* rf = new QFormLayout(robotGroup);
    robotIp_ = new QLineEdit;
    rf->addRow("Default robot IPv4", robotIp_);
    l->addWidget(robotGroup);

    auto* cg = new QGroupBox("Cameras (Tapo C100: stream1 = HD, stream2 = SD; URL override for ESP32-CAM etc.)");
    auto* cl = new QVBoxLayout(cg);
    cams_ = new QTableWidget(0, ColCount);
    cams_->setHorizontalHeaderLabels({"On", "Name", "Role", "Host / IP", "Port", "Stream", "URL override"});
    cams_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    cams_->horizontalHeader()->setSectionResizeMode(ColEnabled, QHeaderView::ResizeToContents);
    cams_->setMinimumHeight(220);
    cl->addWidget(cams_);
    auto* cb = new QHBoxLayout;
    auto* add = new QPushButton("Add camera");
    auto* remove = new QPushButton("Remove last");
    cb->addWidget(add);
    cb->addWidget(remove);
    cb->addStretch();
    cl->addLayout(cb);
    l->addWidget(cg);

    auto* jg = new QGroupBox("Jira / Xray");
    auto* jf = new QFormLayout(jg);
    deployment_ = new QComboBox;
    deployment_->addItem("Jira Cloud + Xray Cloud", "cloud");
    deployment_->addItem("Jira Data Center + Xray Server/DC", "datacenter");
    jiraUrl_ = new QLineEdit;
    jiraUrl_->setPlaceholderText("https://yourcompany.atlassian.net");
    xrayUrl_ = new QLineEdit;
    project_ = new QLineEdit;
    jql_ = new QLineEdit;
    jql_->setPlaceholderText("Default: project = KEY AND issuetype = Test");
    jf->addRow("Deployment", deployment_);
    jf->addRow("Jira base URL", jiraUrl_);
    jf->addRow("Xray Cloud URL", xrayUrl_);
    jf->addRow("Project key", project_);
    jf->addRow("Test JQL", jql_);
    l->addWidget(jg);

    auto* pg = new QGroupBox("Platform & paths");
    auto* pf = new QFormLayout(pg);
    platformMode_ = new QComboBox;
    platformMode_->addItem("None (operator observation only)", "none");
    platformMode_->addItem("SIMULATED (development only)", "simulated");
    allowObservation_ = new QCheckBox("Allow operator-observed outcomes when no platform is connected");
    evidence_ = new QLineEdit;
    evidence_->setPlaceholderText(ctx_.config->resolvedEvidenceDir());
    definitions_ = new QLineEdit;
    definitions_->setPlaceholderText("config/test_definitions");
    bindings_ = new QLineEdit;
    bindings_->setPlaceholderText("config/test_bindings.json");
    pf->addRow("Platform client", platformMode_);
    pf->addRow("", allowObservation_);
    pf->addRow("Evidence folder", evidence_);
    pf->addRow("Test definitions", definitions_);
    pf->addRow("Jira bindings file", bindings_);
    l->addWidget(pg);

    auto* sg = new QGroupBox("Secrets (environment)");
    auto* sl = new QVBoxLayout(sg);
    secrets_ = new QLabel;
    secrets_->setTextFormat(Qt::RichText);
    sl->addWidget(secrets_);
    l->addWidget(sg);
    l->addStretch();

    auto* save = new QPushButton("Save settings");
    save->setObjectName("primary");
    auto* sb = new QHBoxLayout;
    sb->addStretch();
    sb->addWidget(save);
    outer->addLayout(sb);

    connect(add, &QPushButton::clicked, this, &SettingsPage::addCameraRow);
    connect(remove, &QPushButton::clicked, this, [this]() {
        if (cams_->rowCount() > 4) cams_->removeRow(cams_->rowCount() - 1);
    });
    connect(save, &QPushButton::clicked, this, &SettingsPage::save);
    populate();
}

void SettingsPage::addCameraRow() {
    const int r = cams_->rowCount();
    cams_->insertRow(r);
    auto* on = new QTableWidgetItem;
    on->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    on->setCheckState(Qt::Unchecked);
    cams_->setItem(r, ColEnabled, on);
    cams_->setItem(r, ColName, new QTableWidgetItem(QString("Camera %1").arg(r + 1)));
    cams_->setItem(r, ColRole, new QTableWidgetItem("arm"));
    cams_->setItem(r, ColHost, new QTableWidgetItem);
    cams_->setItem(r, ColPort, new QTableWidgetItem("554"));
    cams_->setItem(r, ColStream, new QTableWidgetItem("stream2"));
    cams_->setItem(r, ColUrl, new QTableWidgetItem);
}

void SettingsPage::populate() {
    const AppConfig& c = *ctx_.config;
    robotIp_->setText(c.robotIp);
    cams_->setRowCount(0);
    for (const auto& cam : c.cameras) {
        addCameraRow();
        const int r = cams_->rowCount() - 1;
        cams_->item(r, ColEnabled)->setCheckState(cam.enabled ? Qt::Checked : Qt::Unchecked);
        cams_->item(r, ColName)->setText(cam.name);
        cams_->item(r, ColRole)->setText(cam.role);
        cams_->item(r, ColHost)->setText(cam.host);
        cams_->item(r, ColPort)->setText(QString::number(cam.port));
        cams_->item(r, ColStream)->setText(cam.stream);
        cams_->item(r, ColUrl)->setText(cam.url);
    }
    deployment_->setCurrentIndex(c.jira.deployment == "datacenter" ? 1 : 0);
    jiraUrl_->setText(c.jira.baseUrl);
    xrayUrl_->setText(c.jira.xrayBaseUrl);
    project_->setText(c.jira.projectKey);
    jql_->setText(c.jira.testJql);
    platformMode_->setCurrentIndex(c.platform.mode == "simulated" ? 1 : 0);
    allowObservation_->setChecked(c.platform.allowOperatorObservation);
    evidence_->setText(c.evidenceDir);
    definitions_->setText(c.testDefinitionsDir);
    bindings_->setText(c.bindingsFile);

    QString s;
    for (const char* name : kSecretNames)
        s += QString("%1: %2<br>").arg(QString::fromLatin1(name),
                                       AppConfig::hasSecret(name) ? QString("<span style='color:#56d364'>set</span>")
                                                                  : QString("<span style='color:#91a5bf'>not set</span>"));
    s += "Per-camera overrides: RG_CAM1_USER / RG_CAM1_PASSWORD ... RG_CAMn_*";
    secrets_->setText(s);
}

void SettingsPage::save() {
    AppConfig c = *ctx_.config;
    const QString ip = robotIp_->text().trimmed();
    QHostAddress addr;
    if (!addr.setAddress(ip) || addr.protocol() != QAbstractSocket::IPv4Protocol) {
        QMessageBox::warning(this, "Invalid", "Robot IP must be an IPv4 address.");
        return;
    }
    c.robotIp = ip;
    c.cameras.clear();
    for (int r = 0; r < cams_->rowCount(); ++r) {
        CameraConfig cam;
        cam.id = QString("cam%1").arg(r + 1);
        cam.enabled = cams_->item(r, ColEnabled)->checkState() == Qt::Checked;
        cam.name = cams_->item(r, ColName)->text().trimmed();
        cam.role = cams_->item(r, ColRole)->text().trimmed();
        cam.host = cams_->item(r, ColHost)->text().trimmed();
        bool ok = false;
        cam.port = cams_->item(r, ColPort)->text().toInt(&ok);
        if (!ok || cam.port <= 0 || cam.port > 65535) {
            QMessageBox::warning(this, "Invalid", QString("Camera %1: invalid port").arg(r + 1));
            return;
        }
        cam.stream = cams_->item(r, ColStream)->text().trimmed();
        cam.url = cams_->item(r, ColUrl)->text().trimmed();
        if (cam.url.contains('@')) {
            QMessageBox::warning(this, "Invalid",
                                 QString("Camera %1: do not put credentials in the URL override; use RG_CAM%1_USER / "
                                         "RG_CAM%1_PASSWORD").arg(r + 1));
            return;
        }
        c.cameras.push_back(cam);
    }
    c.jira.deployment = deployment_->currentData().toString();
    c.jira.baseUrl = jiraUrl_->text().trimmed();
    c.jira.xrayBaseUrl = xrayUrl_->text().trimmed();
    c.jira.projectKey = project_->text().trimmed();
    c.jira.testJql = jql_->text().trimmed();
    c.platform.mode = platformMode_->currentData().toString();
    c.platform.allowOperatorObservation = allowObservation_->isChecked();
    c.evidenceDir = evidence_->text().trimmed();
    c.testDefinitionsDir = definitions_->text().trimmed();
    c.bindingsFile = bindings_->text().trimmed();

    QString err;
    if (!c.save(&err)) {
        QMessageBox::warning(this, "Save failed", err);
        return;
    }
    ctx_.log->info("Settings saved to " + AppConfig::userConfigPath());
    QMessageBox::information(this, "Saved", "Settings saved. Restart Robo Guide to apply them.");
}
