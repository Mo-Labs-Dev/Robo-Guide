#include "PlatformClient.h"

#include <QTimer>

using rg::core::Outcome;
using rg::core::PlatformEvent;

void NullPlatformClient::fetchEvents(int requestId, const EventQuery&) {
    QTimer::singleShot(0, this, [this, requestId]() {
        emit eventsReady(requestId, {}, "No platform interface configured");
    });
}

void NullPlatformClient::fetchDevices(int requestId) {
    QTimer::singleShot(0, this, [this, requestId]() {
        emit devicesReady(requestId, {}, "No platform interface configured");
    });
}

void SimulatedPlatformClient::fetchEvents(int requestId, const EventQuery& q) {
    const Outcome outcome = next_;
    QTimer::singleShot(300, this, [this, requestId, q, outcome]() {
        QVector<PlatformEvent> events;
        if (outcome != Outcome::NoEvent) {
            PlatformEvent e;
            e.eventId = QString("SIM-%1").arg(requestId).toStdString();
            e.readerId = q.readerId.toStdString();
            e.credentialId = q.credentialId.toStdString();
            e.outcome = outcome;
            e.timestampMs = q.fromMs + (q.toMs - q.fromMs) / 3;
            events.push_back(e);
        }
        emit eventsReady(requestId, events, QString());
    });
}

void SimulatedPlatformClient::fetchDevices(int requestId) {
    QTimer::singleShot(200, this, [this, requestId]() {
        QVector<PlatformDevice> d = {
            {"READER-1", "Simulated reader 1", "reader", "online"},
            {"READER-2", "Simulated reader 2", "reader", "offline"},
            {"HANDLE-1", "Simulated door handle", "electronic handle", "online"},
        };
        emit devicesReady(requestId, d, QString());
    });
}

PlatformClient* createPlatformClient(const QString& mode, QObject* parent) {
    if (mode == "simulated") return new SimulatedPlatformClient(parent);
    // Add the approved company client here, e.g.:
    // if (mode == "primeweb_rest") return new PrimeWebRestClient(config, parent);
    return new NullPlatformClient(parent);
}
