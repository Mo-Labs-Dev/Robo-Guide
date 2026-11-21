// Company access-management platform integration (read-only).
//
// The real interface (REST API, event interface, approved DB view, ...) is not known yet and
// must come from the company's documentation and permissions. Implement a new subclass of
// PlatformClient for it; nothing else in the application needs to change.
#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include "robo_guide/core/verdict.hpp"

struct PlatformDevice {
    QString id;
    QString name;
    QString type;    // reader, door handle, knob cylinder, locker lock, ...
    QString status;  // online/offline/... as reported
};

struct EventQuery {
    QString readerId;
    QString credentialId;  // hint only; correlation happens in the core
    qint64 fromMs = 0;
    qint64 toMs = 0;
};

class PlatformClient : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~PlatformClient() override = default;

    virtual QString name() const = 0;
    virtual bool isAvailable() const = 0;
    virtual bool isSimulated() const { return false; }

    // Asynchronous; each call results in exactly one eventsReady / devicesReady with the same requestId.
    virtual void fetchEvents(int requestId, const EventQuery& query) = 0;
    virtual void fetchDevices(int requestId) = 0;

signals:
    void eventsReady(int requestId, const QVector<rg::core::PlatformEvent>& events, const QString& error);
    void devicesReady(int requestId, const QVector<PlatformDevice>& devices, const QString& error);
};

// Used when no platform interface is configured.
class NullPlatformClient : public PlatformClient {
    Q_OBJECT
public:
    using PlatformClient::PlatformClient;
    QString name() const override { return "Not configured"; }
    bool isAvailable() const override { return false; }
    void fetchEvents(int requestId, const EventQuery& query) override;
    void fetchDevices(int requestId) override;
};

// Development aid ONLY. Generates events locally so the pipeline (correlation, verdicts,
// evidence, reports) can be exercised without the real platform. Results produced with it are
// marked SIMULATED and are refused for Xray publishing.
class SimulatedPlatformClient : public PlatformClient {
    Q_OBJECT
public:
    using PlatformClient::PlatformClient;
    QString name() const override { return "SIMULATED platform"; }
    bool isAvailable() const override { return true; }
    bool isSimulated() const override { return true; }
    void fetchEvents(int requestId, const EventQuery& query) override;
    void fetchDevices(int requestId) override;

    void setNextOutcome(rg::core::Outcome o) { next_ = o; }
    rg::core::Outcome nextOutcome() const { return next_; }

private:
    rg::core::Outcome next_ = rg::core::Outcome::AccessGranted;
};

PlatformClient* createPlatformClient(const QString& mode, QObject* parent);
