#include "CameraThread.h"

#include <QElapsedTimer>
#include <QMutexLocker>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <vector>

CameraThread::CameraThread(int index, QString url, QString displayUrl, QObject* parent)
    : QThread(parent), index_(index), url_(std::move(url)), displayUrl_(std::move(displayUrl)) {}

CameraThread::~CameraThread() {
    requestInterruption();
    wait(8000);
}

QImage CameraThread::latestFrame() const {
    QMutexLocker lock(&mutex_);
    return latest_;
}

void CameraThread::setRecordingPath(const QString& path) {
    QMutexLocker lock(&mutex_);
    recordPath_ = path;
}

bool CameraThread::sleepInterruptible(int ms) {
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
        if (isInterruptionRequested()) return false;
        msleep(100);
    }
    return true;
}

static QImage toQImage(const cv::Mat& bgr) {
    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
    return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
}

void CameraThread::run() {
    int backoffMs = 2000;
    while (!isInterruptionRequested()) {
        emit stateChanged(index_, "Connecting...", false);
        cv::VideoCapture cap;
        const std::vector<int> params = {cv::CAP_PROP_OPEN_TIMEOUT_MSEC, 6000, cv::CAP_PROP_READ_TIMEOUT_MSEC, 6000};
        bool opened = false;
        try {
            opened = cap.open(url_.toStdString(), cv::CAP_FFMPEG, params);
        } catch (const cv::Exception&) {
            opened = false;
        }
        if (!opened || !cap.isOpened()) {
            emit stateChanged(index_, QString("Unavailable - retrying in %1 s").arg(backoffMs / 1000), false);
            if (!sleepInterruptible(backoffMs)) break;
            backoffMs = qMin(backoffMs * 2, 30000);
            continue;
        }
        backoffMs = 2000;
        connected_ = true;

        double streamFps = cap.get(cv::CAP_PROP_FPS);
        if (!(streamFps > 1.0 && streamFps < 61.0)) streamFps = 15.0;

        cv::VideoWriter writer;
        QString writerPath;
        cv::Mat frame;
        int failures = 0;
        int framesInWindow = 0;
        QElapsedTimer previewTimer, fpsTimer;
        previewTimer.start();
        fpsTimer.start();
        bool announced = false;

        while (!isInterruptionRequested()) {
            bool ok = false;
            try {
                ok = cap.read(frame);
            } catch (const cv::Exception&) {
                ok = false;
            }
            if (!ok || frame.empty()) {
                if (++failures > 20) break;
                msleep(50);
                continue;
            }
            failures = 0;
            ++framesInWindow;
            if (fpsTimer.elapsed() >= 2000) {
                fps_ = framesInWindow * 1000.0 / static_cast<double>(fpsTimer.elapsed());
                framesInWindow = 0;
                fpsTimer.restart();
            }
            if (!announced) {
                emit stateChanged(index_, QString("Live %1x%2").arg(frame.cols).arg(frame.rows), true);
                announced = true;
            }

            QString wantedPath;
            {
                QMutexLocker lock(&mutex_);
                wantedPath = recordPath_;
            }
            if (wantedPath != writerPath) {
                if (writer.isOpened()) writer.release();
                writerPath = wantedPath;
                if (!writerPath.isEmpty()) {
                    writer.open(writerPath.toStdString(), cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), streamFps,
                                frame.size());
                    emit stateChanged(index_, writer.isOpened() ? "Live - recording" : "Live - recording FAILED", true);
                } else {
                    emit stateChanged(index_, QString("Live %1x%2").arg(frame.cols).arg(frame.rows), true);
                }
            }
            if (writer.isOpened()) writer.write(frame);

            // Preview and snapshot frame at ~8 fps to keep four streams cheap in the GUI.
            if (previewTimer.elapsed() >= 120) {
                previewTimer.restart();
                QImage img = toQImage(frame);
                {
                    QMutexLocker lock(&mutex_);
                    latest_ = img;
                }
                emit frameReady(index_, img);
            }
        }
        if (writer.isOpened()) writer.release();
        cap.release();
        connected_ = false;
        fps_ = 0.0;
        if (isInterruptionRequested()) break;
        emit stateChanged(index_, "Stream lost - reconnecting", false);
        if (!sleepInterruptible(1000)) break;
    }
    connected_ = false;
    emit stateChanged(index_, "Stopped", false);
}
