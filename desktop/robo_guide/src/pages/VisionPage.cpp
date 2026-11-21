#include "VisionPage.h"

#include <QDateTime>
#include <QDir>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>

#include "ActivityLog.h"
#include "AppConfig.h"
#include "CameraManager.h"
#include "UiHelpers.h"

VisionPage::VisionPage(const AppContext& ctx, QWidget* parent) : QWidget(parent), ctx_(ctx) {
    auto* l = ui::makePage(this, "Vision Studio");
    l->addWidget(ui::note("Live RTSP preview of the Tapo C100 cameras (and an optional ESP32-CAM via HTTP). "
                          "Snapshots and recordings are stored in the evidence folder. Object detection and "
                          "camera-to-robot calibration are Phase 2 and not implemented yet."));

    auto* bar = new QHBoxLayout;
    auto* startAll = new QPushButton("Start all enabled");
    auto* stopAll = new QPushButton("Stop all");
    auto* snap = new QPushButton("Snapshot all");
    record_ = new QPushButton("Start recording");
    bar->addWidget(startAll);
    bar->addWidget(stopAll);
    bar->addWidget(snap);
    bar->addWidget(record_);
    bar->addStretch();
    l->addLayout(bar);

    auto* grid = new QGridLayout;
    const int n = ctx_.cameras->count();
    for (int i = 0; i < n; ++i) {
        auto* g = new QGroupBox(QString("%1  -  %2").arg(i + 1).arg(ctx_.cameras->name(i)));
        auto* gl = new QVBoxLayout(g);
        Tile t;
        t.image = new QLabel(ctx_.cameras->isEnabled(i) ? "Stopped" : "Disabled in Settings");
        t.image->setAlignment(Qt::AlignCenter);
        t.image->setMinimumSize(320, 180);
        t.image->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        t.image->setStyleSheet("background:#080f19;border-radius:8px;");
        gl->addWidget(t.image, 1);
        auto* row = new QHBoxLayout;
        t.state = new QLabel(ctx_.cameras->displayUrl(i).isEmpty() ? "No host configured" : ctx_.cameras->displayUrl(i));
        t.state->setObjectName("note");
        t.toggle = new QPushButton("Start");
        row->addWidget(t.state, 1);
        row->addWidget(t.toggle);
        gl->addLayout(row);
        grid->addWidget(g, i / 2, i % 2);
        connect(t.toggle, &QPushButton::clicked, this, [this, i]() { toggleCamera(i); });
        tiles_.push_back(t);
    }
    l->addLayout(grid, 1);

    connect(startAll, &QPushButton::clicked, ctx_.cameras, &CameraManager::startAllEnabled);
    connect(stopAll, &QPushButton::clicked, ctx_.cameras, &CameraManager::stopAll);
    connect(snap, &QPushButton::clicked, this, &VisionPage::snapshotAll);
    connect(record_, &QPushButton::clicked, this, &VisionPage::toggleRecording);

    connect(ctx_.cameras, &CameraManager::frameReady, this, [this](int i, const QImage& img) {
        if (i < 0 || i >= tiles_.size()) return;
        QLabel* label = tiles_[i].image;
        label->setPixmap(QPixmap::fromImage(img).scaled(label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    });
    connect(ctx_.cameras, &CameraManager::stateChanged, this, [this](int i, const QString& s, bool connected) {
        if (i < 0 || i >= tiles_.size()) return;
        const QString url = ctx_.cameras->displayUrl(i);
        tiles_[i].state->setText(s + (url.isEmpty() ? QString() : "  -  " + url));
        tiles_[i].toggle->setText(ctx_.cameras->isRunning(i) ? "Stop" : "Start");
        if (!connected) {
            tiles_[i].image->setPixmap(QPixmap());
            tiles_[i].image->setText(s);
        }
    });
}

void VisionPage::toggleCamera(int i) {
    if (ctx_.cameras->isRunning(i)) {
        ctx_.cameras->stop(i);
        return;
    }
    QString err;
    if (!ctx_.cameras->start(i, &err)) ctx_.log->warn(QString("Camera %1: %2").arg(i + 1).arg(err));
    tiles_[i].toggle->setText(ctx_.cameras->isRunning(i) ? "Stop" : "Start");
}

void VisionPage::snapshotAll() {
    const QString dir = ctx_.config->resolvedEvidenceDir() + "/snapshots";
    const QString prefix = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_");
    const QStringList files = ctx_.cameras->snapshotAll(dir, prefix);
    ctx_.log->info(files.isEmpty() ? "Snapshot: no live camera"
                                   : QString("Saved %1 snapshot(s) to %2").arg(files.size()).arg(dir));
}

void VisionPage::toggleRecording() {
    const bool on = !ctx_.cameras->recording();
    ctx_.cameras->setRecording(on, ctx_.config->resolvedEvidenceDir() + "/recordings");
    record_->setText(on ? "Stop recording" : "Start recording");
}
