// Activity log shown in Diagnostics and appended to <AppData>/activity.csv.
// Call only from the GUI thread; worker threads report through signals.
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class ActivityLog : public QObject {
    Q_OBJECT
public:
    explicit ActivityLog(QObject* parent = nullptr);
    void info(const QString& message);
    void warn(const QString& message);
    QStringList recent() const { return recent_; }
    QString filePath() const { return path_; }

signals:
    void appended(const QString& line);

private:
    void write(const QString& level, const QString& message);
    QString path_;
    QStringList recent_;
};
