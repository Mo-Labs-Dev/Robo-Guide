#pragma once

#include <QWidget>

#include "AppContext.h"

class QLabel;
class QPlainTextEdit;
class QTableWidget;

class DiagnosticsPage : public QWidget {
    Q_OBJECT
public:
    explicit DiagnosticsPage(const AppContext& ctx, QWidget* parent = nullptr);

private:
    void refresh();
    AppContext ctx_;
    QPlainTextEdit* log_ = nullptr;
    QTableWidget* cams_ = nullptr;
    QLabel* info_ = nullptr;
};
