#include "PlatformPage.h"

#include <QComboBox>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>

#include "AppConfig.h"
#include "PlatformClient.h"
#include "UiHelpers.h"

using namespace rg::core;

// Request ids for this page live in a separate range from the test runner's.
static int g_pageRequest = 1000000;

PlatformPage::PlatformPage(const AppContext& ctx, QWidget* parent) : QWidget(parent), ctx_(ctx) {
    auto* l = ui::makePage(this, "Platform Integration");
    l->addWidget(ui::note(
        "Read-only connection to the company access-management test environment. The real interface (REST API, "
        "event interface or approved read access) must be taken from company documentation and approved before it "
        "is implemented - see docs/INTEGRATIONS.md. Configuration-changing operations are out of scope."));

    status_ = new QLabel;
    ui::setPill(status_, ctx_.platform->name(), ctx_.platform->isAvailable() ? (ctx_.platform->isSimulated() ? "warn" : "ok") : "idle");
    auto* sr = new QHBoxLayout;
    sr->addWidget(new QLabel("Client:"));
    sr->addWidget(status_);
    sr->addStretch();
    l->addLayout(sr);

    if (auto* sim = qobject_cast<SimulatedPlatformClient*>(ctx_.platform)) {
        auto* sg = new QGroupBox("Simulator (development only - results are marked SIMULATED)");
        auto* sf = new QFormLayout(sg);
        simOutcome_ = new QComboBox;
        const Outcome all[] = {Outcome::AccessGranted, Outcome::AccessDenied, Outcome::CredentialRecognized,
                               Outcome::CredentialUnknown, Outcome::CommunicationError, Outcome::NoEvent};
        for (auto o : all) simOutcome_->addItem(QString::fromLatin1(toString(o)), static_cast<int>(o));
        sf->addRow("Next simulated outcome", simOutcome_);
        connect(simOutcome_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, sim](int) { sim->setNextOutcome(static_cast<Outcome>(simOutcome_->currentData().toInt())); });
        l->addWidget(sg);
    }

    auto* dg = new QGroupBox("Devices");
    auto* dl = new QVBoxLayout(dg);
    devices_ = new QTableWidget(0, 4);
    devices_->setHorizontalHeaderLabels({"Id", "Name", "Type", "Status"});
    devices_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    devices_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    dl->addWidget(devices_, 1);
    auto* fetchDevices = new QPushButton("Fetch devices");
    dl->addWidget(fetchDevices);
    l->addWidget(dg, 1);

    auto* eg = new QGroupBox("Events");
    auto* el = new QVBoxLayout(eg);
    auto* q = new QHBoxLayout;
    reader_ = new QLineEdit;
    reader_->setPlaceholderText("Reader id");
    seconds_ = new QSpinBox;
    seconds_->setRange(5, 86400);
    seconds_->setValue(300);
    seconds_->setSuffix(" s back");
    auto* fetchEvents = new QPushButton("Query events");
    q->addWidget(reader_, 1);
    q->addWidget(seconds_);
    q->addWidget(fetchEvents);
    el->addLayout(q);
    events_ = new QTableWidget(0, 5);
    events_->setHorizontalHeaderLabels({"Time", "Reader", "Credential", "Outcome", "Event id"});
    events_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    events_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    el->addWidget(events_, 1);
    l->addWidget(eg, 1);

    connect(fetchDevices, &QPushButton::clicked, this, [this]() {
        devicesRequest_ = ++g_pageRequest;
        ctx_.platform->fetchDevices(devicesRequest_);
    });
    connect(fetchEvents, &QPushButton::clicked, this, [this]() {
        EventQuery eq;
        eq.readerId = reader_->text().trimmed();
        eq.toMs = QDateTime::currentMSecsSinceEpoch();
        eq.fromMs = eq.toMs - static_cast<qint64>(seconds_->value()) * 1000;
        eventsRequest_ = ++g_pageRequest;
        ctx_.platform->fetchEvents(eventsRequest_, eq);
    });
    connect(ctx_.platform, &PlatformClient::devicesReady, this,
            [this](int id, const QVector<PlatformDevice>& d, const QString& error) {
                if (id != devicesRequest_) return;
                devices_->setRowCount(d.size());
                for (int r = 0; r < d.size(); ++r) {
                    devices_->setItem(r, 0, new QTableWidgetItem(d[r].id));
                    devices_->setItem(r, 1, new QTableWidgetItem(d[r].name));
                    devices_->setItem(r, 2, new QTableWidgetItem(d[r].type));
                    devices_->setItem(r, 3, new QTableWidgetItem(d[r].status));
                }
                if (!error.isEmpty()) ui::setPill(status_, ctx_.platform->name() + ": " + error, "bad");
            });
    connect(ctx_.platform, &PlatformClient::eventsReady, this,
            [this](int id, const QVector<PlatformEvent>& ev, const QString& error) {
                if (id != eventsRequest_) return;
                events_->setRowCount(ev.size());
                for (int r = 0; r < ev.size(); ++r) {
                    const auto& e = ev[r];
                    events_->setItem(r, 0, new QTableWidgetItem(QDateTime::fromMSecsSinceEpoch(e.timestampMs).toString("yyyy-MM-dd HH:mm:ss.zzz")));
                    events_->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(e.readerId)));
                    events_->setItem(r, 2, new QTableWidgetItem(QString::fromStdString(e.credentialId)));
                    events_->setItem(r, 3, new QTableWidgetItem(QString::fromLatin1(toString(e.outcome))));
                    events_->setItem(r, 4, new QTableWidgetItem(QString::fromStdString(e.eventId)));
                }
                if (!error.isEmpty()) ui::setPill(status_, ctx_.platform->name() + ": " + error, "bad");
            });
}
