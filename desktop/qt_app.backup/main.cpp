#include <QApplication>
#include "mainwindow.h"
#include "rclcpp/rclcpp.hpp"
int main(int argc, char *argv[])
{
   rclcpp::init(argc, argv);
   int result = 0;
   {
       QApplication app(argc, argv);
       MainWindow window;
       window.show();
       result = app.exec();
   }
   rclcpp::shutdown();
   return result;
}
