#include "TestCasePage.h"

#include <QColor>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>

#include "ActivityLog.h"
#include "AppConfig.h"
#include "TestLibrary.h"
#include "UiHelpers.h"

static QTableWidgetItem* cell(const QString& text) {
    auto* it = new QTableWidgetItem(text);
    it->setFlags(it->flags() & ~Qt::ItemIsEditable);
    return it;
}

TestCasePage::TestCasePage(const AppContext& ctx, QWidget* parent) : QWidget(parent), ctx_(ctx) {
    auto* l = ui::makePage(this, "Test Case Manager");
    l->addWidget(ui::note(
        "Only approved test definitions can be executed. A Jira/Xray test is executable when it is bound to a "
        "definition in test_bindings.json; unbound tests are shown as UNSUPPORTED and are never improvised."));

    auto* dg = new QGroupBox("Approved test definitions");
    auto* dl = new QVBoxLayout(dg);
    defsInfo_ = ui::note("");
    dl->addWidget(defsInfo_);
    defs_ = new QTableWidget(0, 6);
    defs_->setHorizontalHeaderLabels({"Id", "Title", "Reader", "Credential", "Expected", "Status"});
    defs_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    defs_->setSelectionBehavior(QAbstractItemView::SelectRows);
    defs_->setSelectionMode(QAbstractItemView::SingleSelection);
    dl->addWidget(defs_, 1);
    auto* db = new QHBoxLayout;
    auto* reload = new QPushButton("Reload definitions");
    auto* use = new QPushButton("Execute selected definition");
    use->setObjectName("primary");
    db->addWidget(reload);
    db->addStretch();
    db->addWidget(use);
    dl->addLayout(db);
    l->addWidget(dg, 1);

    auto* jg = new QGroupBox("Jira / Xray tests");
    auto* jl = new QVBoxLayout(jg);
    jiraInfo_ = ui::note("Not fetched yet.");
    jl->addWidget(jiraInfo_);
    jira_ = new QTableWidget(0, 4);
    jira_->setHorizontalHeaderLabels({"Key", "Summary", "Jira status", "Robo Guide binding"});
    jira_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    jira_->setSelectionBehavior(QAbstractItemView::SelectRows);
    jira_->setSelectionMode(QAbstractItemView::SingleSelection);
    jl->addWidget(jira_, 1);
    auto* jb = new QHBoxLayout;
    auto* fetch = new QPushButton("Fetch tests from Jira");
    auto* useJira = new QPushButton("Execute selected Jira test");
    useJira->setObjectName("primary");
    jb->addWidget(fetch);
    jb->addStretch();
    jb->addWidget(useJira);
    jl->addLayout(jb);
    l->addWidget(jg, 1);

    connect(reload, &QPushButton::clicked, this, &TestCasePage::reloadDefinitions);
    connect(use, &QPushButton::clicked, this, &TestCasePage::useSelectedDefinition);
    connect(fetch, &QPushButton::clicked, this, &TestCasePage::fetchJira);
    connect(useJira, &QPushButton::clicked, this, &TestCasePage::useSelectedJira);
    connect(ctx_.jira, &JiraXrayClient::testsReady, this, &TestCasePage::showJira);
    reloadDefinitions();
}

void TestCasePage::reloadDefinitions() {
    ctx_.tests->load(ctx_.config->resolvedDefinitionsDir(), ctx_.config->resolvedBindingsFile());
    const auto& defs = ctx_.tests->definitions();
    defs_->setRowCount(defs.size());
    int valid = 0;
    for (int r = 0; r < defs.size(); ++r) {
        const auto& d = defs[r];
        defs_->setItem(r, 0, cell(QString::fromStdString(d.def.id)));
        defs_->setItem(r, 1, cell(QString::fromStdString(d.def.title)));
        defs_->setItem(r, 2, cell(QString::fromStdString(d.def.readerId)));
        defs_->setItem(r, 3, cell(QString::fromStdString(d.def.credentialId)));
        defs_->setItem(r, 4, cell(QString::fromLatin1(rg::core::toString(d.def.expected))));
        auto* st = cell(d.valid() ? "VALID" : "INVALID: " + d.problems.join("; "));
        st->setForeground(QColor(d.valid() ? "#56d364" : "#ff7b72"));
        st->setToolTip(d.file + (d.valid() ? QString() : "\n" + d.problems.join("\n")));
        defs_->setItem(r, 5, st);
        if (d.valid()) ++valid;
    }
    QString info = QString("%1 definition(s), %2 valid - %3").arg(defs.size()).arg(valid).arg(ctx_.config->resolvedDefinitionsDir());
    for (const auto& e : ctx_.tests->loadErrors()) info += "\n" + e;
    defsInfo_->setText(info);
    ctx_.log->info(QString("Loaded %1 test definition(s), %2 valid").arg(defs.size()).arg(valid));
}

void TestCasePage::fetchJira() {
    jiraInfo_->setText("Fetching...");
    ctx_.jira->searchTests();
}

void TestCasePage::showJira(const QVector<JiraTest>& tests, const QString& error) {
    jira_->setRowCount(tests.size());
    int bound = 0;
    for (int r = 0; r < tests.size(); ++r) {
        const auto& t = tests[r];
        jira_->setItem(r, 0, cell(t.key));
        jira_->setItem(r, 1, cell(t.summary));
        jira_->setItem(r, 2, cell(t.status));
        const QString def = ctx_.tests->bindingFor(t.key);
        const auto* ld = def.isEmpty() ? nullptr : ctx_.tests->find(def);
        QString text;
        QString color = "#ffa657";
        if (def.isEmpty()) {
            text = "UNSUPPORTED - no approved definition";
        } else if (!ld) {
            text = "BROKEN - definition '" + def + "' not found";
            color = "#ff7b72";
        } else if (!ld->valid()) {
            text = "BLOCKED - definition '" + def + "' invalid";
            color = "#ff7b72";
        } else {
            text = def;
            color = "#56d364";
            ++bound;
        }
        auto* it = cell(text);
        it->setForeground(QColor(color));
        jira_->setItem(r, 3, it);
    }
    QString info = QString("%1 test(s), %2 executable").arg(tests.size()).arg(bound);
    if (!error.isEmpty()) info = error + (tests.isEmpty() ? QString() : "\n" + info);
    jiraInfo_->setText(info);
    if (!error.isEmpty()) ctx_.log->warn(error);
}

void TestCasePage::useSelectedDefinition() {
    const int r = defs_->currentRow();
    if (r < 0) return;
    emit testSelected(defs_->item(r, 0)->text(), QString());
}

void TestCasePage::useSelectedJira() {
    const int r = jira_->currentRow();
    if (r < 0) return;
    const QString key = jira_->item(r, 0)->text();
    const QString def = ctx_.tests->bindingFor(key);
    if (def.isEmpty()) {
        ctx_.log->warn(key + " has no approved definition - marked UNSUPPORTED, not executed");
        return;
    }
    emit testSelected(def, key);
}
