#include "RobotPage.h"

#include <QColor>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>

#include "ActivityLog.h"
#include "AppConfig.h"
#include "RosBridge.h"
#include "UiHelpers.h"

RobotPage::RobotPage(const AppContext& ctx, QWidget* parent) : QWidget(parent), ctx_(ctx) {
    auto* l = ui::makePage(this, "Robot Control");
    auto* row = new QHBoxLayout;
    l->addLayout(row, 1);
    auto* left = new QVBoxLayout;
    auto* right = new QVBoxLayout;
    row->addLayout(left, 1);
    row->addLayout(right, 1);

    auto* conn = new QGroupBox("Connection (read-only monitoring)");
    auto* form = new QFormLayout(conn);
    ip_ = new QLineEdit(ctx_.config->robotIp);
    mode_ = new QComboBox;
    mode_->addItems({"Simulation", "Hardware (read only)"});
    mode_->setCurrentIndex(1);
    form->addRow("Robot IPv4", ip_);
    form->addRow("Mode", mode_);
    auto* buttons = new QHBoxLayout;
    connect_ = new QPushButton("Connect");
    disconnect_ = new QPushButton("Disconnect");
    disconnect_->setEnabled(false);
    buttons->addWidget(connect_);
    buttons->addWidget(disconnect_);
    form->addRow(buttons);
    status_ = new QLabel("DISCONNECTED");
    status_->setStyleSheet("font-size:17px;color:#70b9ff;font-weight:bold;");
    form->addRow("Status", status_);
    left->addWidget(conn);

    auto* jg = new QGroupBox("Joint positions (controller feedback)");
    auto* jl = new QFormLayout(jg);
    const QStringList names = {"Base", "Shoulder", "Elbow", "Hand"};
    for (int i = 0; i < 4; ++i) {
        joints_[i] = new QLabel("-");
        jl->addRow(names[i], joints_[i]);
    }
    left->addWidget(jg);
    left->addWidget(ui::note("Joint readings are raw controller feedback, not calibrated limit indicators."));
    left->addStretch();

    auto* gg = new QGroupBox("Motion safety gate");
    auto* gl = new QVBoxLayout(gg);
    gl->addWidget(ui::note("Physical motion is allowed only when every item below is satisfied. "
                           "There are intentionally no motion controls on this page."));
    gate_ = new QListWidget;
    gl->addWidget(gate_, 1);
    right->addWidget(gg, 1);

    connect(connect_, &QPushButton::clicked, this, &RobotPage::connectRobot);
    connect(disconnect_, &QPushButton::clicked, this, &RobotPage::disconnectRobot);
    connect(ctx_.ros, &RosBridge::statusChanged, this, [this](const QString& s, bool ok) {
        status_->setText(s);
        status_->setStyleSheet(QString("font-size:17px;font-weight:bold;color:%1;").arg(ok ? "#56d364" : "#70b9ff"));
    });
    connect(ctx_.ros, &RosBridge::jointsChanged, this, [this](const QVector<double>& v) {
        for (int i = 0; i < 4; ++i) {
            if (v.size() == 4)
                joints_[i]->setText(QString("%1 rad   |   %2 deg").arg(v[i], 0, 'f', 4).arg(v[i] * 180.0 / 3.141592653589793, 0, 'f', 1));
            else
                joints_[i]->setText("-");
        }
    });
    auto* t = new QTimer(this);
    connect(t, &QTimer::timeout, this, &RobotPage::refreshGate);
    t->start(1000);
    refreshGate();
}

void RobotPage::setConnectedUi(bool monitoring) {
    connect_->setEnabled(!monitoring);
    disconnect_->setEnabled(monitoring);
    mode_->setEnabled(!monitoring);
    ip_->setEnabled(!monitoring);
}

void RobotPage::connectRobot() {
    const bool hardware = mode_->currentIndex() == 1;
    if (hardware) {
        QHostAddress addr;
        if (!addr.setAddress(ip_->text().trimmed()) || addr.protocol() != QAbstractSocket::IPv4Protocol) {
            ctx_.log->warn("Invalid robot IPv4 address");
            status_->setText("Invalid IPv4 address");
            return;
        }
        ctx_.ros->connectHardware(ip_->text().trimmed());
    } else {
        ctx_.ros->connectSimulation();
    }
    setConnectedUi(true);
}

void RobotPage::disconnectRobot() {
    ctx_.ros->disconnectRobot();
    setConnectedUi(false);
}

void RobotPage::refreshGate() {
    const auto d = ctx_.motionGate();
    gate_->clear();
    if (d.allowed) {
        auto* item = new QListWidgetItem("All preconditions satisfied - motion ARMED");
        item->setForeground(QColor("#ff7b72"));
        gate_->addItem(item);
        return;
    }
    for (const auto& reason : d.unmet) {
        auto* item = new QListWidgetItem(QString::fromUtf8("✗  ") + QString::fromStdString(reason));
        item->setForeground(QColor("#ffa657"));
        gate_->addItem(item);
    }
}
