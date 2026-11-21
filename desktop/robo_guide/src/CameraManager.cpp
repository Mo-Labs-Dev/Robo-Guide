#include "CameraManager.h"

#include <QDateTime>
#include <QDir>

#include "AppConfig.h"
#include "CameraThread.h"

CameraManager::CameraManager(const AppConfig* config, QObject* parent) : QObject(parent), config_(config) {
    threads_.resize(config_->cameras.size());
    states_.resize(config_->cameras.size());
    for (auto& s : states_) s = "Stopped";
}

CameraManager::~CameraManager() { stopAll(); }

int CameraManager::count() const { return config_->cameras.size(); }
QString CameraManager::name(int i) const { return config_->cameras.value(i).name; }
QString CameraManager::role(int i) const { return config_->cameras.value(i).role; }
bool CameraManager::isEnabled(int i) const { return config_->cameras.value(i).enabled; }
bool CameraManager::isRunning(int i) const { return i >= 0 && i < threads_.size() && threads_[i] != nullptr; }
bool CameraManager::isConnected(int i) const { return isRunning(i) && threads_[i]->connected(); }
QString CameraManager::state(int i) const { return states_.value(i); }
QString CameraManager::displayUrl(int i) const { return config_->cameraUrl(i, true); }
double CameraManager::fps(int i) const { return isRunning(i) ? threads_[i]->measuredFps() : 0.0; }

int CameraManager::connectedCount() const {
    int n = 0;
    for (int i = 0; i < threads_.size(); ++i)
        if (isConnected(i)) ++n;
    return n;
}

bool CameraManager::start(int i, QString* error) {
    if (i < 0 || i >= threads_.size()) return false;
    if (threads_[i]) return true;
    const QString url = config_->cameraUrl(i, false);
    if (url.isEmpty()) {
        if (error) *error = "Camera has no host or URL configured (Settings)";
        states_[i] = "Not configured";
        emit stateChanged(i, states_[i], false);
        return false;
    }
    auto* t = new CameraThread(i, url, config_->cameraUrl(i, true), this);
    connect(t, &CameraThread::frameReady, this, &CameraManager::frameReady);
    connect(t, &CameraThread::stateChanged, this, [this](int idx, const QString& s, bool c) {
        if (idx >= 0 && idx < states_.size()) states_[idx] = s;
        emit stateChanged(idx, s, c);
    });
    threads_[i] = t;
    if (recording_) {
        // Join the active recording session.
        const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        t->setRecordingPath(QDir(recordingDir_).filePath(QString("%1_cam%2.avi").arg(stamp).arg(i + 1)));
    }
    t->start();
    emit logMessage(QString("Camera %1 starting: %2").arg(i + 1).arg(t->displayUrl()));
    return true;
}

void CameraManager::stop(int i) {
    if (!isRunning(i)) return;
    CameraThread* t = threads_[i];
    threads_[i] = nullptr;
    t->requestInterruption();
    if (!t->wait(8000)) emit logMessage(QString("Camera %1 did not stop within 8 s").arg(i + 1));
    t->deleteLater();
    states_[i] = "Stopped";
    emit stateChanged(i, states_[i], false);
}

void CameraManager::startAllEnabled() {
    for (int i = 0; i < count(); ++i)
        if (isEnabled(i)) start(i);
}

void CameraManager::stopAll() {
    for (int i = 0; i < threads_.size(); ++i) stop(i);
}

bool CameraManager::snapshot(int i, const QString& filePath) {
    if (!isRunning(i)) return false;
    const QImage img = threads_[i]->latestFrame();
    return !img.isNull() && img.save(filePath, "JPG", 90);
}

QStringList CameraManager::snapshotAll(const QString& dir, const QString& prefix) {
    QStringList written;
    QDir().mkpath(dir);
    for (int i = 0; i < threads_.size(); ++i) {
        if (!isConnected(i)) continue;
        const QString path = QDir(dir).filePath(QString("%1cam%2_%3.jpg").arg(prefix).arg(i + 1).arg(role(i)));
        if (snapshot(i, path)) written << path;
    }
    return written;
}

void CameraManager::setRecording(bool on, const QString& dir) {
    recording_ = on;
    recordingDir_ = dir;
    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    if (on) QDir().mkpath(dir);
    for (int i = 0; i < threads_.size(); ++i) {
        if (!threads_[i]) continue;
        threads_[i]->setRecordingPath(on ? QDir(dir).filePath(QString("%1_cam%2.avi").arg(stamp).arg(i + 1)) : QString());
    }
    emit logMessage(on ? "Recording started: " + dir : "Recording stopped");
}
