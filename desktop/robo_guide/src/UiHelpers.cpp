#include "UiHelpers.h"

namespace ui {

QString styleSheet() {
    return "QMainWindow,QWidget{background:#101826;color:#e5edf8;font-size:13px;}"
           "QPushButton{background:#24344b;border:1px solid #344962;border-radius:7px;padding:8px 12px;}"
           "QPushButton:hover{background:#305178;}QPushButton:disabled{color:#667789;}"
           "QPushButton#nav{text-align:left;padding:10px 14px;}"
           "QPushButton#nav:checked{background:#305178;border-color:#70b9ff;}"
           "QPushButton#primary{background:#1f6feb;border-color:#388bfd;font-weight:bold;}"
           "QLineEdit,QComboBox,QTextEdit,QPlainTextEdit,QTableWidget,QListWidget,QSpinBox{background:#172337;"
           "border:1px solid #344962;border-radius:6px;padding:5px;color:#e5edf8;}"
           "QHeaderView::section{background:#1b2a40;color:#cfe0f5;border:none;padding:6px;}"
           "QGroupBox{border:1px solid #344962;border-radius:9px;margin-top:14px;padding:12px;font-weight:bold;}"
           "QGroupBox::title{subcontrol-origin:margin;left:12px;padding:0 6px;}"
           "QLabel#note{color:#91a5bf;font-weight:normal;}"
           "QLabel#heading{font-size:24px;font-weight:700;}"
           "QLabel#brand{font-size:20px;font-weight:800;color:#70b9ff;}"
           "QLabel#pill{border-radius:6px;padding:4px 8px;font-weight:bold;}";
}

QVBoxLayout* makePage(QWidget* page, const QString& heading) {
    auto* l = new QVBoxLayout(page);
    l->setContentsMargins(22, 18, 22, 18);
    auto* t = new QLabel(heading);
    t->setObjectName("heading");
    l->addWidget(t);
    return l;
}

QLabel* note(const QString& text) {
    auto* l = new QLabel(text);
    l->setObjectName("note");
    l->setWordWrap(true);
    return l;
}

void setPill(QLabel* label, const QString& text, const QString& level) {
    QString bg = "#2a3647", fg = "#cfe0f5";
    if (level == "ok") { bg = "#12391f"; fg = "#56d364"; }
    else if (level == "warn") { bg = "#3d2e00"; fg = "#e3b341"; }
    else if (level == "bad") { bg = "#4a1214"; fg = "#ff7b72"; }
    label->setObjectName("pill");
    label->setText(text);
    label->setStyleSheet(QString("background:%1;color:%2;border-radius:6px;padding:4px 8px;font-weight:bold;").arg(bg, fg));
}

QString verdictColor(const QString& v) {
    if (v == "PASS") return "#56d364";
    if (v == "FAIL") return "#ff7b72";
    if (v == "BLOCKED") return "#e3b341";
    return "#ffa657";  // ERROR
}

}  // namespace ui
