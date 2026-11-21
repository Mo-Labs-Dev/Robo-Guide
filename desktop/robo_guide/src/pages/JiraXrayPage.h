#pragma once

#include <QWidget>

#include "AppContext.h"

class QLabel;
class QPlainTextEdit;

class JiraXrayPage : public QWidget {
    Q_OBJECT
public:
    explicit JiraXrayPage(const AppContext& ctx, QWidget* parent = nullptr);

private:
    void refresh();
    void importFile();

    AppContext ctx_;
    QLabel* summary_ = nullptr;
    QPlainTextEdit* log_ = nullptr;
};
