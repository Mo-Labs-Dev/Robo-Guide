#include "JiraXrayClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>

#include "AppConfig.h"

namespace {
constexpr int kPageSize = 100;
constexpr int kMaxTests = 2000;
constexpr int kTimeoutMs = 30000;
}  // namespace

JiraXrayClient::JiraXrayClient(const AppConfig* config, QObject* parent) : QObject(parent), config_(config) {}

bool JiraXrayClient::isCloud() const { return config_->jira.deployment != "datacenter"; }

bool JiraXrayClient::urlAllowed(const QString& url, QString* why) const {
    const QUrl u(url);
    if (!u.isValid() || u.host().isEmpty()) {
        if (why) *why = "Invalid URL: " + url;
        return false;
    }
    if (u.scheme() != "https" && !(u.scheme() == "http" && config_->jira.allowInsecureHttp)) {
        if (why) *why = "Only https:// is allowed (set jira.allowInsecureHttp for a lab server)";
        return false;
    }
    return true;
}

bool JiraXrayClient::jiraConfigured(QString* why) const {
    if (config_->jira.baseUrl.isEmpty()) {
        if (why) *why = "jira.baseUrl is not set";
        return false;
    }
    if (!urlAllowed(config_->jira.baseUrl, why)) return false;
    if (!AppConfig::hasSecret("RG_JIRA_TOKEN")) {
        if (why) *why = "RG_JIRA_TOKEN is not set";
        return false;
    }
    if (isCloud() && !AppConfig::hasSecret("RG_JIRA_USER")) {
        if (why) *why = "RG_JIRA_USER (account e-mail) is required for Jira Cloud";
        return false;
    }
    return true;
}

bool JiraXrayClient::xrayConfigured(QString* why) const {
    if (isCloud()) {
        if (!urlAllowed(config_->jira.xrayBaseUrl, why)) return false;
        if (!AppConfig::hasSecret("RG_XRAY_CLIENT_ID") || !AppConfig::hasSecret("RG_XRAY_CLIENT_SECRET")) {
            if (why) *why = "RG_XRAY_CLIENT_ID / RG_XRAY_CLIENT_SECRET are not set";
            return false;
        }
        return true;
    }
    return jiraConfigured(why);  // Xray Server/DC uses Jira authentication
}

QNetworkRequest JiraXrayClient::jiraRequest(const QUrl& url) const {
    QNetworkRequest req(url);
    req.setTransferTimeout(kTimeoutMs);
    req.setRawHeader("Accept", "application/json");
    const QString user = AppConfig::secret("RG_JIRA_USER");
    const QString token = AppConfig::secret("RG_JIRA_TOKEN");
    if (!user.isEmpty()) {
        req.setRawHeader("Authorization", "Basic " + (user + ":" + token).toUtf8().toBase64());
    } else {
        req.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    }
    return req;
}

QString JiraXrayClient::replyError(QNetworkReply* r) {
    const int http = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString body = QString::fromUtf8(r->readAll().left(400)).simplified();
    QString msg = http ? QString("HTTP %1").arg(http) : r->errorString();
    if (!body.isEmpty()) msg += " - " + body;
    return msg;
}

void JiraXrayClient::searchTests() {
    QString why;
    if (!jiraConfigured(&why)) {
        emit testsReady({}, why);
        return;
    }
    QString jql = config_->jira.testJql;
    if (jql.isEmpty()) {
        if (config_->jira.projectKey.isEmpty()) {
            emit testsReady({}, "Set jira.projectKey or jira.testJql");
            return;
        }
        jql = QString("project = \"%1\" AND issuetype = Test ORDER BY key").arg(config_->jira.projectKey);
    }
    searchPage(jql, QString(), 0, {});
}

void JiraXrayClient::searchPage(const QString& jql, const QString& pageToken, int startAt, QVector<JiraTest> acc) {
    QString base = config_->jira.baseUrl;
    while (base.endsWith('/')) base.chop(1);
    QUrl url(base + (isCloud() ? "/rest/api/3/search/jql" : "/rest/api/2/search"));
    QUrlQuery q;
    q.addQueryItem("jql", jql);
    q.addQueryItem("fields", "summary,status");
    q.addQueryItem("maxResults", QString::number(kPageSize));
    if (isCloud()) {
        if (!pageToken.isEmpty()) q.addQueryItem("nextPageToken", pageToken);
    } else {
        q.addQueryItem("startAt", QString::number(startAt));
    }
    url.setQuery(q);

    QNetworkReply* reply = nam_.get(jiraRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, jql, startAt, acc]() mutable {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit testsReady(acc, "Jira search failed: " + replyError(reply));
            return;
        }
        const auto root = QJsonDocument::fromJson(reply->readAll()).object();
        const auto issues = root.value("issues").toArray();
        for (const auto& v : issues) {
            const auto o = v.toObject();
            const auto f = o.value("fields").toObject();
            acc.push_back({o.value("key").toString(), f.value("summary").toString(),
                           f.value("status").toObject().value("name").toString()});
        }
        if (acc.size() >= kMaxTests || issues.isEmpty()) {
            emit testsReady(acc, QString());
            return;
        }
        if (isCloud()) {
            const QString next = root.value("nextPageToken").toString();
            if (next.isEmpty() || root.value("isLast").toBool(false)) {
                emit testsReady(acc, QString());
                return;
            }
            searchPage(jql, next, 0, acc);
        } else {
            const int total = root.value("total").toInt();
            const int nextStart = startAt + static_cast<int>(issues.size());
            if (nextStart >= total) {
                emit testsReady(acc, QString());
                return;
            }
            searchPage(jql, QString(), nextStart, acc);
        }
    });
}

void JiraXrayClient::authenticateXray(std::function<void(const QString&, const QString&)> done) {
    QString base = config_->jira.xrayBaseUrl;
    while (base.endsWith('/')) base.chop(1);
    QNetworkRequest req(QUrl(base + "/api/v2/authenticate"));
    req.setTransferTimeout(kTimeoutMs);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    const QJsonObject body{{"client_id", AppConfig::secret("RG_XRAY_CLIENT_ID")},
                           {"client_secret", AppConfig::secret("RG_XRAY_CLIENT_SECRET")}};
    QNetworkReply* reply = nam_.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply, done]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            done(QString(), "Xray authentication failed: " + replyError(reply));
            return;
        }
        // The token is returned as a JSON string literal: "eyJ..."
        QString token = QString::fromUtf8(reply->readAll()).trimmed();
        if (token.startsWith('"') && token.endsWith('"') && token.size() >= 2) token = token.mid(1, token.size() - 2);
        if (token.isEmpty()) {
            done(QString(), "Xray authentication returned an empty token");
            return;
        }
        done(token, QString());
    });
}

void JiraXrayClient::importExecution(const QByteArray& xrayJson) {
    QString why;
    if (!xrayConfigured(&why)) {
        emit importFinished(false, why);
        return;
    }
    auto handleReply = [this](QNetworkReply* reply) {
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                emit importFinished(false, "Xray import failed: " + replyError(reply));
                return;
            }
            const auto o = QJsonDocument::fromJson(reply->readAll()).object();
            QString key = o.value("key").toString();
            if (key.isEmpty()) key = o.value("testExecIssue").toObject().value("key").toString();
            emit importFinished(true, key.isEmpty() ? "Imported" : "Imported into " + key);
        });
    };

    if (isCloud()) {
        authenticateXray([this, xrayJson, handleReply](const QString& token, const QString& err) {
            if (!err.isEmpty()) {
                emit importFinished(false, err);
                return;
            }
            QString base = config_->jira.xrayBaseUrl;
            while (base.endsWith('/')) base.chop(1);
            QNetworkRequest req(QUrl(base + "/api/v2/import/execution"));
            req.setTransferTimeout(kTimeoutMs);
            req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            req.setRawHeader("Authorization", "Bearer " + token.toUtf8());
            handleReply(nam_.post(req, xrayJson));
        });
    } else {
        QString base = config_->jira.baseUrl;
        while (base.endsWith('/')) base.chop(1);
        QNetworkRequest req = jiraRequest(QUrl(base + "/rest/raven/1.0/import/execution"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        handleReply(nam_.post(req, xrayJson));
    }
}
