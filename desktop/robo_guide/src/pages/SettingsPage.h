#pragma once

#include <QWidget>

#include "AppContext.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QTableWidget;

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(const AppContext& ctx, QWidget* parent = nullptr);

private:
    void populate();
    void save();
    void addCameraRow();

    AppContext ctx_;
    QLineEdit* robotIp_ = nullptr;
    QTableWidget* cams_ = nullptr;
    QComboBox* deployment_ = nullptr;
    QLineEdit *jiraUrl_ = nullptr, *xrayUrl_ = nullptr, *project_ = nullptr, *jql_ = nullptr;
    QComboBox* platformMode_ = nullptr;
    QCheckBox* allowObservation_ = nullptr;
    QLineEdit *evidence_ = nullptr, *definitions_ = nullptr, *bindings_ = nullptr;
    QLabel* secrets_ = nullptr;
};
