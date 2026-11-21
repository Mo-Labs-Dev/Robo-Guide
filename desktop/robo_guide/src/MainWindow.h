#pragma once

#include <QMainWindow>
#include <QVector>

#include "AppContext.h"

class QLabel;
class QPushButton;
class QStackedWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(AppConfig* config, QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void showPage(int index);

    AppContext ctx_;
    QStackedWidget* pages_ = nullptr;
    QVector<QPushButton*> nav_;
    QLabel* footer_ = nullptr;
};
