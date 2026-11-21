#include "RosBridge.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

RosBridge::RosBridge(QObject* parent) : QObject(parent) {
    node_ = std::make_shared<rclcpp::Node>("robo_guide_gui");
    commands_ = node_->create_publisher<std_msgs::msg::String>(kCommandTopic, 10);

    statusSub_ = node_->create_subscription<std_msgs::msg::String>(
        kStatusTopic, 10, [this](std_msgs::msg::String::SharedPtr m) {
            if (!monitoring_) return;
            const QString s = QString::fromStdString(m->data);
            if (!s.startsWith(expected_) && !s.startsWith("ERROR")) return;
            statusAge_.restart();
            const bool ok = s.contains(" | CONNECTED") || s.contains(" | READY");
            setStatus(s, ok);
            if (lastLogged_ != s) {
                emit logMessage("Robot: " + s);
                lastLogged_ = s;
            }
            if (!ok) clearJoints();
        });

    jointsSub_ = node_->create_subscription<sensor_msgs::msg::JointState>(
        kJointTopic, 10, [this](sensor_msgs::msg::JointState::SharedPtr m) {
            if (!monitoring_ || !healthy_) return;
            static const char* names[] = {"base", "shoulder", "elbow", "hand"};
            QVector<double> values(4, 0.0);
            for (int i = 0; i < 4; ++i) {
                bool found = false;
                for (size_t j = 0; j < m->name.size() && j < m->position.size(); ++j) {
                    if (m->name[j] == names[i]) {
                        values[i] = m->position[j];
                        found = std::isfinite(values[i]);
                        break;
                    }
                }
                if (!found) return;  // ignore incomplete samples
            }
            joints_ = values;
            jointAge_.restart();
            emit jointsChanged(joints_);
        });

    connect(&spinTimer_, &QTimer::timeout, this, &RosBridge::spinOnce);
    spinTimer_.start(50);
}

RosBridge::~RosBridge() {
    spinTimer_.stop();
    if (monitoring_) send("disconnect");
    jointsSub_.reset();
    statusSub_.reset();
    commands_.reset();
    node_.reset();
}

void RosBridge::send(const QString& action, const QString& ip) {
    if (!commands_) return;
    QJsonObject j{{"action", action}};
    if (!ip.isEmpty()) j["ip"] = ip;
    std_msgs::msg::String m;
    m.data = QJsonDocument(j).toJson(QJsonDocument::Compact).toStdString();
    commands_->publish(m);
}

void RosBridge::connectHardware(const QString& ip) {
    expected_ = "HARDWARE";
    monitoring_ = true;
    lastLogged_.clear();
    clearJoints();
    statusAge_.start();
    setStatus("HARDWARE | Connecting", false);
    send("hardware", ip);
    emit logMessage("Connection request: HARDWARE (read-only) " + ip);
}

void RosBridge::connectSimulation() {
    expected_ = "SIMULATION";
    monitoring_ = true;
    lastLogged_.clear();
    clearJoints();
    statusAge_.start();
    setStatus("SIMULATION | Connecting", false);
    send("simulation");
    emit logMessage("Connection request: SIMULATION");
}

void RosBridge::disconnectRobot() {
    send("disconnect");
    monitoring_ = false;
    expected_.clear();
    lastLogged_.clear();
    statusAge_.invalidate();
    clearJoints();
    setStatus("DISCONNECTED", false);
    emit logMessage("Robot monitoring disconnected");
}

void RosBridge::setStatus(const QString& text, bool healthy) {
    const bool changed = text != statusText_ || healthy != healthy_;
    statusText_ = text;
    healthy_ = healthy;
    if (changed) emit statusChanged(statusText_, healthy_);
}

void RosBridge::clearJoints() {
    jointAge_.invalidate();
    if (!joints_.isEmpty()) {
        joints_.clear();
        emit jointsChanged(joints_);
    }
}

void RosBridge::spinOnce() {
    if (rclcpp::ok()) rclcpp::spin_some(node_);
    if (!monitoring_) return;
    if (statusAge_.isValid() && statusAge_.elapsed() > 4000) {
        clearJoints();
        setStatus("DRIVER STATUS TIMEOUT - is start_driver.sh running?", false);
    } else if (healthy_ && jointAge_.isValid() && jointAge_.elapsed() > 3000) {
        clearJoints();
        setStatus("JOINT FEEDBACK TIMEOUT", false);
    }
}
