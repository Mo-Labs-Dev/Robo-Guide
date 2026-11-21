#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QTimer>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStackedWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QByteArray>
#include <QFile>
#include <QImage>
#include <memory>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
class MainWindow:public QMainWindow {
 Q_OBJECT
public:
 explicit MainWindow(QWidget *parent=nullptr);
 ~MainWindow()override;
private:
 QStackedWidget *pages_=nullptr;
 QLabel *status_=nullptr,*health_=nullptr,*cameraLabel_=nullptr,*cameraState_=nullptr;
 QLabel *joints_[4]={};
 QLineEdit *ip_=nullptr,*cameraUrl_=nullptr;
 QComboBox *mode_=nullptr;
 QTextEdit *log_=nullptr,*taskEditor_=nullptr;
 QPushButton *connect_=nullptr,*disconnect_=nullptr,*cameraStart_=nullptr;
 QTableWidget *taskTable_=nullptr;
 QTimer *spinTimer_=nullptr,*cameraTimer_=nullptr,*taskTimer_=nullptr;
 QElapsedTimer statusAge_,jointAge_;
 QNetworkAccessManager network_;
 QNetworkReply *cameraReply_=nullptr;
 QByteArray mjpegBuffer_;
 bool monitoring_=false,healthy_=false,cameraRunning_=false,taskRunning_=false;
 int taskIndex_=0;
 QString expected_,lastStatus_;
 rclcpp::Node::SharedPtr node_;
 rclcpp::Publisher<std_msgs::msg::String>::SharedPtr commands_;
 rclcpp::Subscription<std_msgs::msg::String>::SharedPtr statusSub_;
 rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr jointsSub_;
 void ui();
 void ros();
 void command(const QString &action,const QString &ip={});
 void connectRobot();
 void disconnectRobot();
 void log(const QString &message);
 void clearJoints();
 void startCamera();
 void stopCamera();
 void processMjpeg();
 void saveTask();
 void loadTask();
 void runDryTask();
 void nextDryTask();
 void stopDryTask();
 void refreshTaskTable();
};
