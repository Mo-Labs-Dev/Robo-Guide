// Owns all camera threads configured in AppConfig.
#pragma once

#include <QImage>
#include <QObject>
#include <QString>
#include <QVector>

class AppConfig;
class CameraThread;

class CameraManager : public QObject {
    Q_OBJECT
public:
    CameraManager(const AppConfig* config, QObject* parent = nullptr);
    ~CameraManager() override;

    int count() const;
    QString name(int index) const;
    QString role(int index) const;
    bool isEnabled(int index) const;
    bool isRunning(int index) const;
    bool isConnected(int index) const;
    int connectedCount() const;
    QString state(int index) const;
    QString displayUrl(int index) const;
    double fps(int index) const;

    bool start(int index, QString* error = nullptr);
    void stop(int index);
    void startAllEnabled();
    void stopAll();

    // Saves the latest frame of every connected camera as JPEG into dir.
    // Returns the written file paths.
    QStringList snapshotAll(const QString& dir, const QString& prefix);
    bool snapshot(int index, const QString& filePath);

    // Starts or stops recording on every running camera; files go to dir.
    void setRecording(bool on, const QString& dir);
    bool recording() const { return recording_; }

signals:
    void frameReady(int index, const QImage& image);
    void stateChanged(int index, const QString& state, bool connected);
    void logMessage(const QString& message);

private:
    const AppConfig* config_;
    QVector<CameraThread*> threads_;
    QVector<QString> states_;
    bool recording_ = false;
    QString recordingDir_;
};
