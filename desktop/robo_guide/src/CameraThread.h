// One camera stream (RTSP or HTTP MJPEG) decoded with OpenCV/FFmpeg in its own thread.
// Reconnects with back-off, throttles preview frames, keeps the latest full-resolution
// frame for snapshots, and records to MJPG/AVI on request.
#pragma once

#include <QImage>
#include <QMutex>
#include <QString>
#include <QThread>
#include <atomic>

class CameraThread : public QThread {
    Q_OBJECT
public:
    // url may contain credentials; it is never logged. displayUrl is the redacted form.
    CameraThread(int index, QString url, QString displayUrl, QObject* parent = nullptr);
    ~CameraThread() override;

    int index() const { return index_; }
    QString displayUrl() const { return displayUrl_; }
    QImage latestFrame() const;            // thread-safe; null if none yet
    void setRecordingPath(const QString& path);  // empty = stop recording; thread-safe
    bool connected() const { return connected_.load(); }
    double measuredFps() const { return fps_.load(); }

signals:
    void frameReady(int index, const QImage& preview);
    void stateChanged(int index, const QString& state, bool connected);

protected:
    void run() override;

private:
    bool sleepInterruptible(int ms);

    const int index_;
    const QString url_;
    const QString displayUrl_;
    mutable QMutex mutex_;
    QImage latest_;
    QString recordPath_;
    std::atomic<bool> connected_{false};
    std::atomic<double> fps_{0.0};
};
