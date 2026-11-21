#include "mainwindow.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QWidget>
#include <QGroupBox>
#include <QDateTime>
#include <QStringList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>
MainWindow::MainWindow(QWidget *parent)
   : QMainWindow(parent)
{
   setupUI();
   setupROS();
}
MainWindow::~MainWindow()
{
   if (rosTimer_)
       rosTimer_->stop();
   if (monitoring_ && commandPublisher_)
       sendCommand("disconnect");
   jointSubscriber_.reset();
   statusSubscriber_.reset();
   commandPublisher_.reset();
   rosNode_.reset();
}
void MainWindow::appendLog(const QString &message)
{
   logView_->append(
       QDateTime::currentDateTime().toString("HH:mm:ss")
       + " | " + message);
}
void MainWindow::clearJoints()
{
   for (auto *label : jointLabels_)
       label->setText("--");
}
void MainWindow::setupUI()
{
   setWindowTitle("RoboTest Studio | Real Robot Monitoring");
   resize(1050, 750);
   auto *central = new QWidget(this);
   auto *layout = new QVBoxLayout(central);
   auto *title = new QLabel("RoboTest Studio");
   title->setStyleSheet(
       "font-size: 26px; font-weight: bold;");
   layout->addWidget(title);
   auto *connectionGroup = new QGroupBox("Robot Connection");
   auto *connectionLayout = new QGridLayout(connectionGroup);
   ipEdit_ = new QLineEdit("192.168.178.31");
   modeCombo_ = new QComboBox();
   modeCombo_->addItem("Simulation");
   modeCombo_->addItem("Hardware (Read Only)");
   modeCombo_->setCurrentIndex(1);
   connectButton_ = new QPushButton("Connect");
   disconnectButton_ = new QPushButton("Disconnect");
   rosStatusLabel_ = new QLabel("ROS 2: Initializing...");
   robotStatusLabel_ = new QLabel("Robot: Disconnected");
   connectionLayout->addWidget(new QLabel("Robot IP:"), 0, 0);
   connectionLayout->addWidget(ipEdit_, 0, 1);
   connectionLayout->addWidget(new QLabel("Mode:"), 1, 0);
   connectionLayout->addWidget(modeCombo_, 1, 1);
   connectionLayout->addWidget(connectButton_, 2, 0);
   connectionLayout->addWidget(disconnectButton_, 2, 1);
   connectionLayout->addWidget(rosStatusLabel_, 3, 0, 1, 2);
   connectionLayout->addWidget(robotStatusLabel_, 4, 0, 1, 2);
   layout->addWidget(connectionGroup);
   auto *jointGroup = new QGroupBox("Live Joint Positions");
   auto *jointLayout = new QGridLayout(jointGroup);
   const QStringList names = {
       "Base", "Shoulder", "Elbow", "Hand"
   };
   for (int i = 0; i < 4; ++i)
   {
       jointLabels_[i] = new QLabel("--");
       jointLayout->addWidget(new QLabel(names[i]), i, 0);
       jointLayout->addWidget(jointLabels_[i], i, 1);
   }
   layout->addWidget(jointGroup);
   auto *logGroup = new QGroupBox("Activity Log");
   auto *logLayout = new QVBoxLayout(logGroup);
   logView_ = new QTextEdit();
   logView_->setReadOnly(true);
   logLayout->addWidget(logView_);
   layout->addWidget(logGroup);
   setCentralWidget(central);
   connect(connectButton_, &QPushButton::clicked,
           this, &MainWindow::connectRobot);
   connect(disconnectButton_, &QPushButton::clicked,
           this, &MainWindow::disconnectRobot);
   connect(modeCombo_,
           QOverload<int>::of(&QComboBox::currentIndexChanged),
           this, [this](int)
           {
               if (monitoring_)
                   disconnectRobot();
               updateControls();
           });
   updateControls();
}
void MainWindow::updateControls()
{
   const bool hardware = modeCombo_->currentIndex() == 1;
   ipEdit_->setEnabled(hardware && !monitoring_);
   modeCombo_->setEnabled(!monitoring_);
   connectButton_->setEnabled(!monitoring_);
   disconnectButton_->setEnabled(monitoring_);
}
void MainWindow::sendCommand(
   const QString &action, const QString &ip)
{
   if (!commandPublisher_)
       return;
   QJsonObject object;
   object["action"] = action;
   if (!ip.isEmpty())
       object["ip"] = ip;
   const QJsonDocument document(object);
   std_msgs::msg::String message;
   message.data =
       document.toJson(QJsonDocument::Compact).toStdString();
   commandPublisher_->publish(message);
}
void MainWindow::connectRobot()
{
   const bool hardware = modeCombo_->currentIndex() == 1;
   if (hardware)
   {
       QHostAddress address;
       if (!address.setAddress(ipEdit_->text().trimmed()) ||
           address.protocol() != QAbstractSocket::IPv4Protocol)
       {
           appendLog("Invalid IPv4 address");
           return;
       }
   }
   expectedMode_ = hardware ? "HARDWARE" : "SIMULATION";
   monitoring_ = true;
   feedbackHealthy_ = false;
   lastStatus_.clear();
   clearJoints();
   statusAge_.start();
   jointAge_.invalidate();
   robotStatusLabel_->setText(
       hardware ? "Hardware: Connecting..."
                : "Simulation: Connecting...");
   sendCommand(
       hardware ? "hardware" : "simulation",
       hardware ? ipEdit_->text().trimmed() : QString());
   appendLog("Connection request sent");
   updateControls();
}
void MainWindow::disconnectRobot()
{
   sendCommand("disconnect");
   monitoring_ = false;
   feedbackHealthy_ = false;
   expectedMode_.clear();
   lastStatus_.clear();
   statusAge_.invalidate();
   jointAge_.invalidate();
   clearJoints();
   robotStatusLabel_->setText("Robot: Disconnected");
   appendLog("Monitoring disconnected");
   updateControls();
}
void MainWindow::checkTimeouts()
{
   if (!monitoring_)
       return;
   if (statusAge_.isValid() && statusAge_.elapsed() > 4000)
   {
       robotStatusLabel_->setText(
           "Robot: Driver status timeout");
       clearJoints();
       feedbackHealthy_ = false;
       return;
   }
   if (jointAge_.isValid() && jointAge_.elapsed() > 3000)
   {
       if (feedbackHealthy_)
       {
           clearJoints();
           feedbackHealthy_ = false;
           appendLog("Joint feedback timed out");
       }
   }
}
void MainWindow::setupROS()
{
   rosNode_ = std::make_shared<rclcpp::Node>(
       "robotest_studio_gui");
   commandPublisher_ =
       rosNode_->create_publisher<std_msgs::msg::String>(
           "/robotest/robot/connection_command", 10);
   statusSubscriber_ =
       rosNode_->create_subscription<std_msgs::msg::String>(
           "/robotest/robot/status", 10,
           [this](std_msgs::msg::String::SharedPtr msg)
           {
               if (!monitoring_)
                   return;
               const QString status =
                   QString::fromStdString(msg->data);
               if (!status.startsWith(expectedMode_) &&
                   !status.startsWith("ERROR"))
                   return;
               statusAge_.restart();
               robotStatusLabel_->setText(status);
               if (status != lastStatus_)
               {
                   appendLog(status);
                   lastStatus_ = status;
               }
               const bool healthy =
                   status.contains(" | CONNECTED") ||
                   status.contains(" | READY");
               if (!healthy)
               {
                   clearJoints();
                   jointAge_.invalidate();
                   feedbackHealthy_ = false;
               }
               else
               {
                   feedbackHealthy_ = true;
               }
           });
   jointSubscriber_ =
       rosNode_->create_subscription<sensor_msgs::msg::JointState>(
           "/robotest/robot/joint_states", 10,
           [this](sensor_msgs::msg::JointState::SharedPtr msg)
           {
               if (!monitoring_ || !feedbackHealthy_)
                   return;
               const QStringList names = {
                   "base", "shoulder", "elbow", "hand"
               };
               constexpr double RAD_TO_DEG =
                   180.0 / 3.14159265358979323846;
               for (int i = 0; i < names.size(); ++i)
               {
                   int index = -1;
                   for (size_t j = 0; j < msg->name.size(); ++j)
                   {
                       if (QString::fromStdString(msg->name[j])
                           == names[i])
                       {
                           index = static_cast<int>(j);
                           break;
                       }
                   }
                   if (index < 0 ||
                       static_cast<size_t>(index) >=
                           msg->position.size())
                       continue;
                   const double rad = msg->position[index];
                   jointLabels_[i]->setText(
                       QString("%1 rad | %2°")
                           .arg(rad, 0, 'f', 3)
                           .arg(rad * RAD_TO_DEG, 0, 'f', 1));
               }
               jointAge_.restart();
           });
   rosTimer_ = new QTimer(this);
   connect(rosTimer_, &QTimer::timeout,
           this, [this]()
           {
               if (rclcpp::ok())
                   rclcpp::spin_some(rosNode_);
               checkTimeouts();
           });
   rosTimer_->start(50);
   rosStatusLabel_->setText(
       "ROS 2 Jazzy: Middleware initialized");
   appendLog("ROS 2 middleware initialized");
}
