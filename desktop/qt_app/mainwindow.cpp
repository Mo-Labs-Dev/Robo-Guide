#include "mainwindow.h"
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkRequest>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <cmath>
static QWidget* panel(const QString &heading,QVBoxLayout **out){
 auto *w=new QWidget;auto *l=new QVBoxLayout(w);l->setContentsMargins(22,18,22,18);
 auto *t=new QLabel(heading);t->setStyleSheet("font-size:24px;font-weight:700;");
 l->addWidget(t);*out=l;return w;
}
MainWindow::MainWindow(QWidget *parent):QMainWindow(parent){
 ui();ros();
 QSettings s("RoboTest","Studio");
 ip_->setText(s.value("ip","192.168.178.31").toString());
 cameraUrl_->setText(s.value("cameraUrl","").toString());
 log("Application ready — hardware motion locked");
}
MainWindow::~MainWindow(){
 stopDryTask();stopCamera();
 if(spinTimer_)spinTimer_->stop();
 if(monitoring_)command("disconnect");
 QSettings s("RoboTest","Studio");s.setValue("ip",ip_->text());s.setValue("cameraUrl",cameraUrl_->text());
 jointsSub_.reset();statusSub_.reset();commands_.reset();node_.reset();
}
void MainWindow::log(const QString &message){
 if(log_)log_->append(QDateTime::currentDateTime().toString("HH:mm:ss")+"  "+message.toHtmlEscaped());
 const QString folder=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
 QDir().mkpath(folder);
 QFile f(folder+"/activity.csv");
 if(f.open(QIODevice::Append|QIODevice::Text)){
  QTextStream stream(&f);
  QString clean=message;clean.replace('"', "\"\"");
  stream<<QDateTime::currentDateTime().toString(Qt::ISODate)<<",\""<<clean<<"\"\n";
 }
}
void MainWindow::clearJoints(){for(auto *l:joints_)if(l)l->setText("—");}
void MainWindow::ui(){
 setWindowTitle("RoboTest Studio | Industrial Robotics Workbench");
 resize(1250,800);
 setStyleSheet("QMainWindow,QWidget{background:#101826;color:#e5edf8;font-size:13px;}"
 "QPushButton{background:#24344b;border:1px solid #344962;border-radius:7px;padding:10px;}"
 "QPushButton:hover{background:#305178;}QPushButton:disabled{color:#667789;}"
 "QLineEdit,QComboBox,QTextEdit,QTableWidget{background:#172337;border:1px solid #344962;border-radius:6px;padding:7px;color:#e5edf8;}"
 "QGroupBox{border:1px solid #344962;border-radius:9px;margin-top:14px;padding:14px;font-weight:bold;}"
 "QGroupBox::title{subcontrol-origin:margin;left:12px;padding:0 6px;}"
 "QLabel#subtitle{color:#91a5bf;}");
 auto *central=new QWidget;auto *outer=new QHBoxLayout(central);outer->setContentsMargins(0,0,0,0);
 auto *sidebar=new QWidget;sidebar->setFixedWidth(205);sidebar->setStyleSheet("background:#0b1320;");
 auto *nav=new QVBoxLayout(sidebar);nav->setContentsMargins(14,20,14,14);
 auto *brand=new QLabel("ROB OTEST\nSTUDIO");brand->setText("ROB OTEST\nSTUDIO");
 brand->setStyleSheet("font-size:19px;font-weight:800;color:#70b9ff;");nav->addWidget(brand);
 nav->addSpacing(25);
 pages_=new QStackedWidget;
 const QStringList names={"Dashboard","Robot Monitor","Vision Studio","Task Designer","Diagnostics","Settings"};
 for(int i=0;i<names.size();++i){
  auto *b=new QPushButton(names[i]);b->setMinimumHeight(44);
  nav->addWidget(b);connect(b,&QPushButton::clicked,this,[this,i](){pages_->setCurrentIndex(i);});
 }
 nav->addStretch();nav->addWidget(new QLabel("V1.0  |  READ-ONLY\nMotion control locked"));
 outer->addWidget(sidebar);outer->addWidget(pages_,1);setCentralWidget(central);
 QVBoxLayout *dash,*robot,*vision,*tasks,*diag,*settings;
 pages_->addWidget(panel("Dashboard",&dash));
 pages_->addWidget(panel("Robot Monitor",&robot));
 pages_->addWidget(panel("Vision Studio",&vision));
 pages_->addWidget(panel("Task Designer — Dry Run",&tasks));
 pages_->addWidget(panel("Diagnostics",&diag));
 pages_->addWidget(panel("Settings",&settings));
 auto *intro=new QLabel("Waveshare RoArm-M2-S  •  ROS 2 Jazzy  •  C++ / Qt 6");
 intro->setObjectName("subtitle");dash->addWidget(intro);
 auto *card=new QGroupBox("System Status");auto *cl=new QVBoxLayout(card);
 status_=new QLabel("DISCONNECTED");status_->setStyleSheet("font-size:19px;color:#70b9ff;font-weight:bold;");
 health_=new QLabel("ROS 2 middleware initializing");
 cl->addWidget(status_);cl->addWidget(health_);dash->addWidget(card);
 dash->addWidget(new QLabel("Modules: Robot Monitor • Vision Studio • Task Designer • Diagnostics"));
 dash->addWidget(new QLabel("SAFETY: No hardware movement or autonomous manipulation is implemented."));
 dash->addStretch();
 auto *conn=new QGroupBox("Connection");auto *form=new QFormLayout(conn);
 ip_=new QLineEdit;mode_=new QComboBox;mode_->addItems({"Simulation","Hardware (Read Only)"});
 mode_->setCurrentIndex(1);form->addRow("Robot IPv4",ip_);form->addRow("Mode",mode_);
 auto *buttons=new QWidget;auto *br=new QHBoxLayout(buttons);br->setContentsMargins(0,0,0,0);
 connect_=new QPushButton("Connect");disconnect_=new QPushButton("Disconnect");disconnect_->setEnabled(false);
 br->addWidget(connect_);br->addWidget(disconnect_);form->addRow(buttons);robot->addWidget(conn);
 auto *jg=new QGroupBox("Joint Positions (controller feedback)");auto *jl=new QFormLayout(jg);
 const QStringList jnames={"Base","Shoulder","Elbow","Hand"};
 for(int i=0;i<4;++i){joints_[i]=new QLabel("—");jl->addRow(jnames[i],joints_[i]);}
 robot->addWidget(jg);robot->addWidget(new QLabel("Joint readings are not calibrated limit indicators."));
 robot->addStretch();
 connect(connect_,&QPushButton::clicked,this,&MainWindow::connectRobot);
 connect(disconnect_,&QPushButton::clicked,this,&MainWindow::disconnectRobot);
 auto *cg=new QGroupBox("ESP32-CAM Network Stream");auto *cgl=new QVBoxLayout(cg);
 cameraUrl_=new QLineEdit;cameraUrl_->setPlaceholderText("http://CAMERA_IP:81/stream  or  http://CAMERA_IP/capture");
 cgl->addWidget(cameraUrl_);
 auto *cb=new QHBoxLayout;cameraStart_=new QPushButton("Start Camera");auto *stop=new QPushButton("Stop Camera");
 cb->addWidget(cameraStart_);cb->addWidget(stop);cgl->addLayout(cb);
 cameraLabel_=new QLabel("Camera not connected");cameraLabel_->setMinimumSize(600,320);
 cameraLabel_->setAlignment(Qt::AlignCenter);cameraLabel_->setStyleSheet("background:#080f19;border-radius:10px;");
 cgl->addWidget(cameraLabel_,1);cameraState_=new QLabel("Camera URL must be configured");cgl->addWidget(cameraState_);
 vision->addWidget(cg,1);vision->addWidget(new QLabel("Vision preview only. Object recognition and hand-eye calibration not implemented."));
 connect(cameraStart_,&QPushButton::clicked,this,&MainWindow::startCamera);
 connect(stop,&QPushButton::clicked,this,&MainWindow::stopCamera);
 auto *tg=new QGroupBox("Task JSON");auto *tl=new QVBoxLayout(tg);
 taskEditor_=new QTextEdit;
 taskEditor_->setPlainText("{\n  \"name\": \"Example inspection\",\n  \"steps\": [\n    {\"type\":\"note\",\"label\":\"Check fixture\"},\n    {\"type\":\"wait\",\"ms\":500},\n    {\"type\":\"note\",\"label\":\"Record inspection result\"}\n  ]\n}");
 tl->addWidget(taskEditor_);
 auto *tb=new QHBoxLayout;
 for(const auto &name:{"Load JSON","Save JSON","Run Dry-Run","Stop"}){
  auto *b=new QPushButton(name);tb->addWidget(b);
  if(QString(name)=="Load JSON")connect(b,&QPushButton::clicked,this,&MainWindow::loadTask);
  if(QString(name)=="Save JSON")connect(b,&QPushButton::clicked,this,&MainWindow::saveTask);
  if(QString(name)=="Run Dry-Run")connect(b,&QPushButton::clicked,this,&MainWindow::runDryTask);
  if(QString(name)=="Stop")connect(b,&QPushButton::clicked,this,&MainWindow::stopDryTask);
 }
 tl->addLayout(tb);tasks->addWidget(tg,2);
 taskTable_=new QTableWidget(0,3);taskTable_->setHorizontalHeaderLabels({"Step","Type","Result"});
 taskTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
 tasks->addWidget(taskTable_,1);
 tasks->addWidget(new QLabel("DRY RUN: Tasks do not send commands to the robot."));
 auto *lg=new QGroupBox("Activity Log");auto *ll=new QVBoxLayout(lg);
 log_=new QTextEdit;log_->setReadOnly(true);ll->addWidget(log_);diag->addWidget(lg,1);
 auto *info=new QLabel("ROS 2 topics:\n/robotest/robot/status\n/robotest/robot/joint_states\n/robotest/robot/connection_command");
 diag->addWidget(info);
 settings->addWidget(new QLabel("IP and camera URL are saved automatically when closing the application."));
 settings->addWidget(new QLabel("Logs: ~/.local/share/RoboTest/Studio/activity.csv"));
 settings->addWidget(new QLabel("Supported camera: HTTP MJPEG or snapshot JPEG. RTSP requires another pipeline."));
 settings->addStretch();
 taskTimer_=new QTimer(this);taskTimer_->setSingleShot(true);
 connect(taskTimer_,&QTimer::timeout,this,&MainWindow::nextDryTask);
}
void MainWindow::command(const QString &action,const QString &address){
 if(!commands_)return;
 QJsonObject j{{"action",action}};if(!address.isEmpty())j["ip"]=address;
 std_msgs::msg::String m;m.data=QJsonDocument(j).toJson(QJsonDocument::Compact).toStdString();
 commands_->publish(m);
}
void MainWindow::connectRobot(){
 bool hardware=mode_->currentIndex()==1;
 QHostAddress addr;
 if(hardware&&(!addr.setAddress(ip_->text().trimmed())||addr.protocol()!=QAbstractSocket::IPv4Protocol)){
  log("Invalid robot IPv4 address");return;
 }
 expected_=hardware?"HARDWARE":"SIMULATION";
 monitoring_=true;healthy_=false;lastStatus_.clear();clearJoints();
 statusAge_.start();jointAge_.invalidate();
 status_->setText(expected_+" | Connecting");
 command(hardware?"hardware":"simulation",hardware?ip_->text().trimmed():QString());
 connect_->setEnabled(false);disconnect_->setEnabled(true);mode_->setEnabled(false);ip_->setEnabled(false);
 log("Connection request: "+expected_);
}
void MainWindow::disconnectRobot(){
 command("disconnect");monitoring_=false;healthy_=false;expected_.clear();lastStatus_.clear();
 clearJoints();status_->setText("DISCONNECTED");statusAge_.invalidate();jointAge_.invalidate();
 connect_->setEnabled(true);disconnect_->setEnabled(false);mode_->setEnabled(true);ip_->setEnabled(true);
 log("Monitoring disconnected");
}
void MainWindow::ros(){
 node_=std::make_shared<rclcpp::Node>("robotest_studio_gui");
 commands_=node_->create_publisher<std_msgs::msg::String>("/robotest/robot/connection_command",10);
 statusSub_=node_->create_subscription<std_msgs::msg::String>("/robotest/robot/status",10,
  [this](std_msgs::msg::String::SharedPtr m){
   if(!monitoring_)return;
   QString s=QString::fromStdString(m->data);
   if(!s.startsWith(expected_)&&!s.startsWith("ERROR"))return;
   statusAge_.restart();status_->setText(s);
   if(lastStatus_!=s){log(s);lastStatus_=s;}
   healthy_=s.contains(" | CONNECTED")||s.contains(" | READY");
   if(!healthy_){clearJoints();jointAge_.invalidate();}
  });
 jointsSub_=node_->create_subscription<sensor_msgs::msg::JointState>("/robotest/robot/joint_states",10,
  [this](sensor_msgs::msg::JointState::SharedPtr m){
   if(!monitoring_||!healthy_)return;
   const QStringList names={"base","shoulder","elbow","hand"};
   bool complete=true;double values[4]={};
   for(int i=0;i<4;++i){
    bool found=false;
    for(size_t j=0;j<m->name.size();++j)if(QString::fromStdString(m->name[j])==names[i]&&j<m->position.size()){
     values[i]=m->position[j];found=std::isfinite(values[i]);break;
    }
    if(!found){complete=false;break;}
   }
   if(!complete)return;
   for(int i=0;i<4;++i)joints_[i]->setText(QString("%1 rad   |   %2°").arg(values[i],0,'f',4).arg(values[i]*180.0/3.141592653589793,0,'f',1));
   jointAge_.restart();
  });
 spinTimer_=new QTimer(this);
 connect(spinTimer_,&QTimer::timeout,this,[this](){
  if(rclcpp::ok())rclcpp::spin_some(node_);
  if(!monitoring_)return;
  if(statusAge_.isValid()&&statusAge_.elapsed()>4000){
   status_->setText("DRIVER STATUS TIMEOUT");healthy_=false;clearJoints();
  }else if(jointAge_.isValid()&&jointAge_.elapsed()>3000){
   clearJoints();healthy_=false;status_->setText("JOINT FEEDBACK TIMEOUT");
  }
 });
 spinTimer_->start(50);health_->setText("ROS 2 Jazzy initialized");
}
void MainWindow::startCamera(){
 stopCamera();
 QUrl url(cameraUrl_->text().trimmed());
 if(!url.isValid()||url.scheme()!="http"||url.host().isEmpty()){
  cameraState_->setText("Enter a valid HTTP camera URL");return;
 }
 cameraRunning_=true;mjpegBuffer_.clear();cameraState_->setText("Connecting to camera...");
 QNetworkRequest req(url);req.setTransferTimeout(12000);
 cameraReply_=network_.get(req);
 connect(cameraReply_,&QNetworkReply::readyRead,this,[this](){
  if(!cameraReply_||!cameraRunning_)return;
  mjpegBuffer_.append(cameraReply_->readAll());
  if(mjpegBuffer_.size()>3000000)mjpegBuffer_=mjpegBuffer_.right(1000000);
  processMjpeg();
 });
 connect(cameraReply_,&QNetworkReply::finished,this,[this](){
  if(!cameraReply_)return;
  if(cameraRunning_){
   if(cameraReply_->error()!=QNetworkReply::NoError)cameraState_->setText(cameraReply_->errorString());
   else cameraState_->setText("Stream ended");
   cameraRunning_=false;
  }
  cameraReply_->deleteLater();cameraReply_=nullptr;
 });
 log("Camera connection requested");
}
void MainWindow::processMjpeg(){
 // Decode JPEG snapshots and JPEG frames within multipart MJPEG.
 while(true){
  int begin=mjpegBuffer_.indexOf("\xFF\xD8",0);
  if(begin<0)return;
  int end=mjpegBuffer_.indexOf("\xFF\xD9",begin+2);
  if(end<0){if(begin>0)mjpegBuffer_.remove(0,begin);return;}
  QByteArray jpg=mjpegBuffer_.mid(begin,end+2-begin);
  mjpegBuffer_.remove(0,end+2);
  QImage img=QImage::fromData(jpg,"JPEG");
  if(!img.isNull()){
   cameraLabel_->setPixmap(QPixmap::fromImage(img).scaled(cameraLabel_->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation));
   cameraState_->setText(QString("Camera receiving frames (%1×%2)").arg(img.width()).arg(img.height()));
  }
 }
}
void MainWindow::stopCamera(){
 cameraRunning_=false;
 if(cameraReply_){cameraReply_->abort();}
 if(cameraState_)cameraState_->setText("Camera stopped");
}
void MainWindow::saveTask(){
 auto doc=QJsonDocument::fromJson(taskEditor_->toPlainText().toUtf8());
 if(!doc.isObject()||!doc.object().value("steps").isArray()){QMessageBox::warning(this,"Invalid task","Task must be JSON object with steps array.");return;}
 auto file=QFileDialog::getSaveFileName(this,"Save task",QDir::homePath()+"/task.json","JSON (*.json)");
 if(file.isEmpty())return;QFile f(file);if(f.open(QIODevice::WriteOnly))f.write(doc.toJson(QJsonDocument::Indented));
}
void MainWindow::loadTask(){
 auto file=QFileDialog::getOpenFileName(this,"Load task",QDir::homePath(),"JSON (*.json)");
 if(file.isEmpty())return;QFile f(file);if(f.open(QIODevice::ReadOnly))taskEditor_->setPlainText(QString::fromUtf8(f.readAll()));
}
void MainWindow::refreshTaskTable(){
 auto doc=QJsonDocument::fromJson(taskEditor_->toPlainText().toUtf8());
 if(!doc.isObject()||!doc.object().value("steps").isArray())return;
 auto arr=doc.object().value("steps").toArray();taskTable_->setRowCount(arr.size());
 for(int i=0;i<arr.size();++i){
  auto obj=arr[i].toObject();
  taskTable_->setItem(i,0,new QTableWidgetItem(QString::number(i+1)));
  taskTable_->setItem(i,1,new QTableWidgetItem(obj.value("type").toString()));
  taskTable_->setItem(i,2,new QTableWidgetItem("PENDING"));
 }
}
void MainWindow::runDryTask(){
 if(taskRunning_)return;
 auto doc=QJsonDocument::fromJson(taskEditor_->toPlainText().toUtf8());
 if(!doc.isObject()||!doc.object().value("steps").isArray()){
  QMessageBox::warning(this,"Invalid task","Expected JSON with a steps array.");return;
 }
 auto steps=doc.object().value("steps").toArray();
 for(const auto &v:steps){
  if(!v.isObject()||!QStringList({"note","wait"}).contains(v.toObject().value("type").toString())){
   QMessageBox::warning(this,"Unsupported step","Dry-run supports only note and wait steps.");return;
  }
 }
 refreshTaskTable();taskRunning_=true;taskIndex_=0;log("Dry-run started (no hardware commands)");nextDryTask();
}
void MainWindow::nextDryTask(){
 if(!taskRunning_)return;
 auto steps=QJsonDocument::fromJson(taskEditor_->toPlainText().toUtf8()).object().value("steps").toArray();
 if(taskIndex_>=steps.size()){taskRunning_=false;log("Dry-run completed");return;}
 auto step=steps[taskIndex_].toObject();
 auto type=step.value("type").toString();
 taskTable_->item(taskIndex_,2)->setText("PASS (SIMULATED)");
 log(QString("Dry-run step %1: %2").arg(taskIndex_+1).arg(type));
 ++taskIndex_;
 int delay=type=="wait"?qBound(0,step.value("ms").toInt(500),5000):350;
 taskTimer_->start(delay);
}
void MainWindow::stopDryTask(){
 if(taskTimer_)taskTimer_->stop();
 if(taskRunning_){taskRunning_=false;log("Dry-run stopped");}
}
