#include "ActivityLog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

ActivityLog::ActivityLog(QObject* parent) : QObject(parent) {
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(folder);
    path_ = folder + "/activity.csv";
}

void ActivityLog::info(const QString& message) { write("INFO", message); }
void ActivityLog::warn(const QString& message) { write("WARN", message); }

void ActivityLog::write(const QString& level, const QString& message) {
    const auto now = QDateTime::currentDateTime();
    const QString line = now.toString("HH:mm:ss") + "  " + (level == "INFO" ? QString() : level + "  ") + message;
    recent_.append(line);
    if (recent_.size() > 2000) recent_.removeFirst();
    emit appended(line);

    QFile f(path_);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream s(&f);
        QString clean = message;
        clean.replace('"', "\"\"");
        s << now.toString(Qt::ISODateWithMs) << "," << level << ",\"" << clean << "\"\n";
    }
}
