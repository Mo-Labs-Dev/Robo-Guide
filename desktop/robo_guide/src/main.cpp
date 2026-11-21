#include <QApplication>
#include <QMessageBox>

#include "AppConfig.h"
#include "MainWindow.h"
#include "rclcpp/rclcpp.hpp"

int main(int argc, char** argv) {
    // RTSP over TCP: UDP RTP rarely survives WSL2 NAT and Wi-Fi packet loss.
    if (qEnvironmentVariableIsEmpty("OPENCV_FFMPEG_CAPTURE_OPTIONS"))
        qputenv("OPENCV_FFMPEG_CAPTURE_OPTIONS", "rtsp_transport;tcp");

    rclcpp::init(argc, argv);
    QApplication app(argc, argv);
    QApplication::setApplicationName("Robo Guide");
    QApplication::setOrganizationName("RoboGuide");
    QApplication::setApplicationDisplayName("Robo Guide");
    app.setStyle("Fusion");

    AppConfig config;
    QString error;
    const bool ok = config.load(&error);

    int result = 0;
    {
        MainWindow window(&config);
        window.show();
        if (!ok) QMessageBox::warning(&window, "Configuration", "Configuration problem, using defaults:\n" + error);
        result = app.exec();
    }
    rclcpp::shutdown();
    return result;
}
