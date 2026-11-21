#pragma once

#include <QWidget>

#include "AppContext.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

class RobotPage : public QWidget {
    Q_OBJECT
public:
    explicit RobotPage(const AppContext& ctx, QWidget* parent = nullptr);

private:
    void connectRobot();
    void disconnectRobot();
    void refreshGate();
    void setConnectedUi(bool monitoring);

    AppContext ctx_;
    QLineEdit* ip_ = nullptr;
    QComboBox* mode_ = nullptr;
    QPushButton *connect_ = nullptr, *disconnect_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* joints_[4] = {};
    QListWidget* gate_ = nullptr;
};
