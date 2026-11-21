#include "DiagnosticsPage.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QTimer>
#include <opencv2/core/version.hpp>

#include "ActivityLog.h"
#include "AppConfig.h"
#include "CameraManager.h"
#include "PlatformClient.h"
#include "RosBridge.h"
#include "UiHelpers.h"

DiagnosticsPage::DiagnosticsPage(const AppContext& ctx, QWidget* parent) : QWidget(parent), ctx_(ctx) {
    auto* l = ui::makePage(this, "Diagnostics");
    auto* row = new QHBoxLayout;
    l->addLayout(row, 1);

    auto* lg = new QGroupBox("Activity log");
    auto* ll = new QVBoxLayout(lg);
    log_ = new QPlainTextEdit;
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(5000);
    for (const auto& line : ctx_.log->recent()) log_->appendPlainText(line);
    ll->addWidget(log_);
    ll->addWidget(ui::note("Also written to " + ctx_.log->filePath()));
    row->addWidget(lg, 3);

    auto* right = new QVBoxLayout;
    row->addLayout(right, 2);
    auto* sg = new QGroupBox("System");
    auto* sl = new QVBoxLayout(sg);
    info_ = new QLabel;
    info_->setWordWrap(true);
    info_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    sl->addWidget(info_);
    right->addWidget(sg);

    auto* cg = new QGroupBox("Cameras");
    auto* cl = new QVBoxLayout(cg);
    cams_ = new QTableWidget(ctx_.cameras->count(), 3);
    cams_->setHorizontalHeaderLabels({"Camera", "State", "FPS"});
    cams_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    cams_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    cl->addWidget(cams_);
    right->addWidget(cg, 1);

    connect(ctx_.log, &ActivityLog::appended, log_, &QPlainTextEdit::appendPlainText);
    auto* t = new QTimer(this);
    connect(t, &QTimer::timeout, this, &DiagnosticsPage::refresh);
    t->start(1000);
    refresh();
}

void DiagnosticsPage::refresh() {
    const auto age = ctx_.ros->jointFeedbackAgeMs();
    info_->setText(QString("Qt %1  -  OpenCV %2\nConfig: %3\nUser config: %4\nEvidence: %5\n\n"
                           "ROS 2 node: robo_guide_gui\nTopics: %6, %7, %8\nRobot: %9")
                       .arg(QString::fromLatin1(qVersion()), QString::fromLatin1(CV_VERSION), ctx_.config->loadedFrom,
                            AppConfig::userConfigPath(), ctx_.config->resolvedEvidenceDir(),
                            QString::fromLatin1(RosBridge::kStatusTopic), QString::fromLatin1(RosBridge::kJointTopic),
                            QString::fromLatin1(RosBridge::kCommandTopic), ctx_.ros->statusText()) +
                   QString("\nJoint feedback age: %1\nPlatform: %2")
                       .arg(age < 0 ? QString("none") : QString("%1 ms").arg(age))
                       .arg(ctx_.platform->name()));
    for (int i = 0; i < ctx_.cameras->count(); ++i) {
        cams_->setItem(i, 0, new QTableWidgetItem(QString("%1 %2").arg(i + 1).arg(ctx_.cameras->name(i))));
        cams_->setItem(i, 1, new QTableWidgetItem(ctx_.cameras->state(i)));
        cams_->setItem(i, 2, new QTableWidgetItem(QString::number(ctx_.cameras->fps(i), 'f', 1)));
    }
}
