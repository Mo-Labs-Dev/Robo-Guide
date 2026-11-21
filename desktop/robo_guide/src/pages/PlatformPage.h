#pragma once

#include <QWidget>

#include "AppContext.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;

class PlatformPage : public QWidget {
    Q_OBJECT
public:
    explicit PlatformPage(const AppContext& ctx, QWidget* parent = nullptr);

private:
    AppContext ctx_;
    QTableWidget* devices_ = nullptr;
    QTableWidget* events_ = nullptr;
    QLineEdit* reader_ = nullptr;
    QSpinBox* seconds_ = nullptr;
    QLabel* status_ = nullptr;
    QComboBox* simOutcome_ = nullptr;
    int devicesRequest_ = 0;
    int eventsRequest_ = 0;
};
