#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QLabel>
#include <QTextEdit>
#include <QTimer>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QElapsedTimer>
#include <memory>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
class MainWindow : public QMainWindow
{
   Q_OBJECT
public:
   explicit MainWindow(QWidget *parent = nullptr);
   ~MainWindow() override;
private:
   QLineEdit *ipEdit_ = nullptr;
   QComboBox *modeCombo_ = nullptr;
   QPushButton *connectButton_ = nullptr;
   QPushButton *disconnectButton_ = nullptr;
   QLabel *robotStatusLabel_ = nullptr;
   QLabel *rosStatusLabel_ = nullptr;
   QLabel *jointLabels_[4] = {};
   QTextEdit *logView_ = nullptr;
   QTimer *rosTimer_ = nullptr;
   QElapsedTimer statusAge_;
   QElapsedTimer jointAge_;
   QString lastStatus_;
   QString expectedMode_;
   bool monitoring_ = false;
   bool feedbackHealthy_ = false;
   rclcpp::Node::SharedPtr rosNode_;
   rclcpp::Publisher<std_msgs::msg::String>::SharedPtr
       commandPublisher_;
   rclcpp::Subscription<std_msgs::msg::String>::SharedPtr
       statusSubscriber_;
   rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr
       jointSubscriber_;
   void setupUI();
   void setupROS();
   void connectRobot();
   void disconnectRobot();
   void sendCommand(const QString &action,
                    const QString &ip = QString());
   void appendLog(const QString &message);
   void updateControls();
   void clearJoints();
   void checkTimeouts();
};
#endif
