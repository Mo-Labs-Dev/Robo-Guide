#include <QApplication>
#include "rclcpp/rclcpp.hpp"
#include "mainwindow.h"
int main(int argc,char **argv){
  rclcpp::init(argc,argv);
  QApplication app(argc,argv);
  app.setStyle("Fusion");
  MainWindow window;window.show();
  int result=app.exec();
  rclcpp::shutdown();return result;
}
