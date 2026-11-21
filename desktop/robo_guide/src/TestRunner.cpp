#include "TestRunner.h"

#include <QDateTime>
#include <QRandomGenerator>
#include <QTimer>

#include "AppConfig.h"
#include "CameraManager.h"
#include "PlatformClient.h"

using namespace rg::core;

static QString qs(const std::string& s) { return QString::fromStdString(s); }

TestRunner::TestRunner(const AppConfig* config, CameraManager* cameras, PlatformClient* platform, QObject* parent)
    : QObject(parent), config_(config), cameras_(cameras), platform_(platform) {
    connect(platform_, &PlatformClient::eventsReady, this,
            [this](int requestId, const QVector<PlatformEvent>& events, const QString& error) {
                if (!running_ || requestId != platformRequest_) return;  // stale reply
                platformRequest_ = 0;
                const QString source = platform_->isSimulated() ? "SIMULATED_PLATFORM" : "PLATFORM";
                record_.events = events;
                if (!error.isEmpty()) {
                    record_.resultSource = source;
                    record_.verdict = Verdict::Error;
                    record_.actual = Outcome::NoEvent;
                    record_.reason = "Platform query failed: " + error;
                    evaluated_ = true;
                    log(record_.reason);
                    stepDone("ERROR");
                    return;
                }
                CorrelationWindow w;
                w.readerId = def_.readerId;
                w.credentialId = def_.credentialId;
                w.startMs = record_.windowStartMs;
                w.endMs = record_.windowEndMs;
                std::vector<PlatformEvent> all(events.begin(), events.end());
                const auto cr = correlate(all, w);
                log(QString("Platform returned %1 event(s), %2 in window: %3")
                        .arg(events.size())
                        .arg(cr.considered.size())
                        .arg(qs(cr.note)));
                applyEvaluation(cr, source);
                stepDone();
            });
}

qint64 TestRunner::nowMs() const { return QDateTime::currentMSecsSinceEpoch(); }

Capabilities TestRunner::capabilities(bool motionAllowed) const {
    Capabilities c;
    c.motionAllowed = motionAllowed;
    c.platformAvailable = platform_->isAvailable();
    c.operatorObservationAllowed = config_->platform.allowOperatorObservation;
    c.camerasConnected = cameras_->connectedCount();
    return c;
}

ExecutionPlan TestRunner::preview(const TestDefinition& def, PresentationMode mode, bool motionAllowed) const {
    return plan(def, capabilities(motionAllowed), mode);
}

void TestRunner::log(const QString& line) {
    const QString stamped = QDateTime::currentDateTime().toString("HH:mm:ss.zzz") + "  " + line;
    record_.stepLog << stamped;
    emit message(line);
}

void TestRunner::ask(PromptKind kind, const QString& text) {
    prompt_ = kind;
    log("OPERATOR: " + text);
    emit prompt(kind, text);
}

bool TestRunner::start(const TestDefinition& def, PresentationMode mode, bool motionAllowed,
                       const QString& jiraTestKey, QString* error) {
    if (running_) {
        if (error) *error = "A test is already running";
        return false;
    }
    ++generation_;
    def_ = def;
    mode_ = mode;
    plan_ = plan(def, capabilities(motionAllowed), mode);
    emit planReady(plan_);

    record_ = ExecutionRecord();
    record_.executionId = QDateTime::currentDateTimeUtc().toString("yyyyMMdd'T'HHmmss") +
                          QString("-%1").arg(QRandomGenerator::global()->bounded(0x10000), 4, 16, QChar('0'));
    record_.testId = qs(def.id);
    record_.testTitle = qs(def.title);
    record_.jiraTestKey = jiraTestKey;
    record_.credentialId = qs(def.credentialId);
    record_.readerId = qs(def.readerId);
    record_.mode = mode == PresentationMode::Robot ? "ROBOT" : "OPERATOR_ASSISTED";
    record_.resultSource = "NONE";
    record_.expected = def.expected;
    record_.started = QDateTime::currentDateTime();
    record_.evidenceDir = Evidence::createExecutionDir(config_->resolvedEvidenceDir(), record_.executionId, record_.testId);

    evaluated_ = false;
    prompt_ = PromptNone;
    index_ = 0;
    platformRequest_ = 0;
    presentStartMs_ = presentEndMs_ = 0;
    running_ = true;

    log(QString("Execution %1 of '%2' (%3) mode %4").arg(record_.executionId, record_.testId, record_.testTitle, record_.mode));
    if (!plan_.runnable) {
        record_.verdict = Verdict::Blocked;
        QStringList problems;
        for (const auto& p : plan_.problems) problems << qs(p);
        record_.reason = "Blocked: " + problems.join("; ");
        log(record_.reason);
        evaluated_ = true;
        const quint64 gen = generation_;
        QTimer::singleShot(0, this, [this, gen]() {
            if (gen == generation_) finalize();
        });
        return true;
    }
    const quint64 gen = generation_;
    QTimer::singleShot(0, this, [this, gen]() {
        if (gen == generation_) runStep();
    });
    return true;
}

void TestRunner::runStep() {
    if (!running_) return;
    if (index_ >= static_cast<int>(plan_.steps.size())) {
        finalize();
        return;
    }
    const PlannedStep& ps = plan_.steps[static_cast<size_t>(index_)];
    const TestStep& s = ps.step;
    const QString label = s.label.empty() ? QString(toString(s.type)) : qs(s.label);
    emit stepChanged(index_, "RUNNING");
    log(QString("Step %1: %2 - %3").arg(index_ + 1).arg(QString::fromLatin1(toString(s.type)), label));

    if (ps.status == StepStatus::Blocked) {  // should not happen for a runnable plan
        abort("Step blocked: " + qs(ps.reason));
        return;
    }
    if (isRobotAction(s.type) && ps.status == StepStatus::Ready) {
        // The motion gate is open but no validated robot executor exists yet.
        abort("Robot execution of physical steps is not implemented yet");
        return;
    }

    const QString cred = record_.credentialId;
    const QString reader = record_.readerId;
    const quint64 gen = generation_;
    switch (s.type) {
        case ActionType::Note:
            stepDone();
            return;
        case ActionType::Wait:
            QTimer::singleShot(qBound(0, s.durationMs, 60000), this, [this, gen]() {
                if (gen == generation_) stepDone();
            });
            return;
        case ActionType::PickCard:
            ask(PromptConfirm, QString("Take credential %1 from its storage position (%2). Confirm when held.").arg(cred, label));
            return;
        case ActionType::ReturnCard:
            ask(PromptConfirm, QString("Return credential %1 to its storage position. Confirm when done.").arg(cred));
            return;
        case ActionType::PresentCard:
            presentStartMs_ = nowMs() - def_.clockSkewMs;
            ask(PromptConfirm, QString("Hold credential %1 in the field of reader %2 now, then click Confirm. "
                                       "Keep it there for %3 s after confirming.")
                                   .arg(cred, reader)
                                   .arg(s.durationMs / 1000.0, 0, 'f', 1));
            return;
        case ActionType::CaptureEvidence: {
            const QStringList files = cameras_->snapshotAll(record_.evidenceDir, QString("step%1_").arg(index_ + 1));
            record_.evidenceFiles << files;
            log(files.isEmpty() ? "No camera connected - no images captured"
                                : QString("Captured %1 camera image(s)").arg(files.size()));
            stepDone();
            return;
        }
        case ActionType::CheckPlatformEvent: {
            record_.windowStartMs = presentStartMs_;
            record_.windowEndMs = presentEndMs_ + def_.eventLatencyMs + def_.clockSkewMs;
            const qint64 waitMs = qMax<qint64>(0, presentEndMs_ + def_.eventLatencyMs - nowMs());
            log(QString("Waiting %1 ms for platform latency").arg(waitMs));
            QTimer::singleShot(static_cast<int>(waitMs), this, [this, gen]() {
                if (gen == generation_) checkPlatform();
            });
            return;
        }
    }
}

void TestRunner::checkPlatform() {
    if (platform_->isAvailable()) {
        EventQuery q;
        q.readerId = record_.readerId;
        q.credentialId = record_.credentialId;
        q.fromMs = record_.windowStartMs;
        q.toMs = record_.windowEndMs;
        platformRequest_ = ++requestCounter_;
        log(QString("Querying %1 for reader %2").arg(platform_->name(), q.readerId));
        platform_->fetchEvents(platformRequest_, q);
        return;
    }
    ask(PromptObserve, QString("No platform connection. Record the outcome you observed for credential %1 on reader %2.")
                           .arg(record_.credentialId, record_.readerId));
}

void TestRunner::applyEvaluation(const CorrelationResult& cr, const QString& source) {
    const auto ev = evaluate(def_.expected, cr, record_.presentationConfirmed);
    record_.resultSource = source;
    record_.verdict = ev.verdict;
    record_.actual = ev.actual;
    record_.reason = qs(ev.reason);
    evaluated_ = true;
    log(QString("Verdict %1: %2").arg(QString::fromLatin1(toString(ev.verdict)), record_.reason));
}

void TestRunner::confirmStep() {
    if (!running_ || prompt_ != PromptConfirm) return;
    prompt_ = PromptNone;
    emit prompt(PromptNone, QString());
    const auto type = plan_.steps[static_cast<size_t>(index_)].step.type;
    log("Operator confirmed");
    if (type == ActionType::PresentCard) {
        record_.presentationConfirmed = true;
        const int hold = plan_.steps[static_cast<size_t>(index_)].step.durationMs;
        const quint64 gen = generation_;
        emit stepChanged(index_, QString("HOLD CARD %1 s").arg(hold / 1000.0, 0, 'f', 1));
        QTimer::singleShot(hold, this, [this, gen]() {
            if (gen != generation_) return;
            presentEndMs_ = nowMs();
            log("Presentation window ended");
            stepDone();
        });
        return;
    }
    stepDone();
}

void TestRunner::submitObservation(Outcome observed) {
    if (!running_ || prompt_ != PromptObserve) return;
    prompt_ = PromptNone;
    emit prompt(PromptNone, QString());
    CorrelationResult cr;
    if (observed == Outcome::NoEvent) {
        cr.kind = CorrelationResult::Kind::NoEvent;
    } else {
        cr.kind = CorrelationResult::Kind::Matched;
        cr.outcome = observed;
    }
    log(QString("Operator observed %1").arg(QString::fromLatin1(toString(observed))));
    applyEvaluation(cr, "OPERATOR_OBSERVED");
    stepDone();
}

void TestRunner::stepDone(const QString& status) {
    if (!running_) return;
    emit stepChanged(index_, status);
    ++index_;
    const quint64 gen = generation_;
    QTimer::singleShot(0, this, [this, gen]() {
        if (gen == generation_) runStep();
    });
}

void TestRunner::abort(const QString& reason) {
    if (!running_) return;
    ++generation_;  // drop pending timers
    platformRequest_ = 0;
    prompt_ = PromptNone;
    emit prompt(PromptNone, QString());
    emit stepChanged(index_, "ABORTED");
    record_.verdict = Verdict::Error;
    record_.reason = "Aborted: " + reason;
    evaluated_ = true;
    log(record_.reason);
    log("If a credential is out of its storage position, return it manually.");
    finalize();
}

void TestRunner::finalize() {
    if (!running_) return;
    if (!evaluated_) {
        record_.verdict = Verdict::Error;
        record_.reason = "The platform check did not complete";
    }
    // A verdict can only be PASS/FAIL when the presentation was confirmed.
    if (!record_.presentationConfirmed && (record_.verdict == Verdict::Pass || record_.verdict == Verdict::Fail)) {
        record_.verdict = Verdict::Error;
        record_.reason = "Presentation not confirmed";
    }
    record_.finished = QDateTime::currentDateTime();
    running_ = false;
    prompt_ = PromptNone;
    QString err;
    if (!Evidence::write(record_, &err)) log("Evidence write failed: " + err);
    log(QString("Finished: %1 - evidence in %2").arg(QString::fromLatin1(toString(record_.verdict)), record_.evidenceDir));
    Evidence::write(record_, &err);  // rewrite so the final log line is included
    emit finished(record_);
}
