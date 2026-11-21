// ROS 2 bridge for the GUI. Talks to the existing read-only Waveshare driver
// (ros2_ws/src/robotest_waveshare) over the unchanged /robotest/robot/* topics.
#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "std_msgs/msg/string.hpp"

class RosBridge : public QObject {
    Q_OBJECT
public:
    explicit RosBridge(QObject* parent = nullptr);
    ~RosBridge() override;

    void connectHardware(const QString& ip);
    void connectSimulation();
    void disconnectRobot();

    bool monitoring() const { return monitoring_; }
    bool healthy() const { return healthy_; }
    bool hardwareMode() const { return expected_ == "HARDWARE"; }
    QString statusText() const { return statusText_; }
    // Age of the newest complete joint sample in ms, or -1 if none.
    qint64 jointFeedbackAgeMs() const { return jointAge_.isValid() ? jointAge_.elapsed() : -1; }
    QVector<double> joints() const { return joints_; }

    static constexpr const char* kStatusTopic = "/robotest/robot/status";
    static constexpr const char* kJointTopic = "/robotest/robot/joint_states";
    static constexpr const char* kCommandTopic = "/robotest/robot/connection_command";

signals:
    void statusChanged(const QString& text, bool healthy);
    void jointsChanged(const QVector<double>& radians);  // empty = cleared
    void logMessage(const QString& message);

private:
    void send(const QString& action, const QString& ip = {});
    void setStatus(const QString& text, bool healthy);
    void clearJoints();
    void spinOnce();

    rclcpp::Node::SharedPtr node_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr commands_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr statusSub_;
    rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr jointsSub_;
    QTimer spinTimer_;
    QElapsedTimer statusAge_, jointAge_;
    bool monitoring_ = false;
    bool healthy_ = false;
    QString expected_;
    QString statusText_ = "DISCONNECTED";
    QString lastLogged_;
    QVector<double> joints_;
};
