// Executes one approved test definition step by step and produces an ExecutionRecord.
//
// Robot steps run only when the motion gate is open. In the default build the gate never
// opens, so physical steps run in OPERATOR-ASSISTED mode: the operator performs the step when
// prompted and confirms it. Everything else (timing windows, platform correlation, verdicts,
// evidence, Xray payloads) is identical to the future robot mode.
#pragma once

#include <QObject>
#include <QString>

#include "ExecutionRecord.h"
#include "robo_guide/core/test_definition.hpp"

class AppConfig;
class CameraManager;
class PlatformClient;

class TestRunner : public QObject {
    Q_OBJECT
public:
    enum PromptKind { PromptNone = 0, PromptConfirm = 1, PromptObserve = 2 };

    TestRunner(const AppConfig* config, CameraManager* cameras, PlatformClient* platform, QObject* parent = nullptr);

    rg::core::Capabilities capabilities(bool motionAllowed) const;
    rg::core::ExecutionPlan preview(const rg::core::TestDefinition& def, rg::core::PresentationMode mode,
                                    bool motionAllowed) const;

    // Starts an execution. If the plan is not runnable, the run finishes immediately as BLOCKED
    // (with evidence) and true is returned. false only if a run is already active.
    bool start(const rg::core::TestDefinition& def, rg::core::PresentationMode mode, bool motionAllowed,
               const QString& jiraTestKey, QString* error);
    void confirmStep();
    void submitObservation(rg::core::Outcome observed);
    void abort(const QString& reason);

    bool isRunning() const { return running_; }
    PromptKind pendingPrompt() const { return prompt_; }
    const ExecutionRecord& lastRecord() const { return record_; }

signals:
    void planReady(const rg::core::ExecutionPlan& plan);
    void stepChanged(int index, const QString& status);
    void prompt(int kind, const QString& text);
    void message(const QString& line);
    void finished(const ExecutionRecord& record);

private:
    void runStep();
    void stepDone(const QString& status = "DONE");
    void checkPlatform();
    void applyEvaluation(const rg::core::CorrelationResult& cr, const QString& source);
    void finalize();
    void log(const QString& line);
    void ask(PromptKind kind, const QString& text);
    qint64 nowMs() const;

    const AppConfig* config_;
    CameraManager* cameras_;
    PlatformClient* platform_;

    rg::core::TestDefinition def_;
    rg::core::ExecutionPlan plan_;
    rg::core::PresentationMode mode_ = rg::core::PresentationMode::OperatorAssisted;
    ExecutionRecord record_;
    bool running_ = false;
    bool evaluated_ = false;
    PromptKind prompt_ = PromptNone;
    int index_ = 0;
    quint64 generation_ = 0;  // invalidates timers/replies from an aborted run
    int platformRequest_ = 0;
    int requestCounter_ = 0;
    qint64 presentStartMs_ = 0;
    qint64 presentEndMs_ = 0;
};
