// Jira + Xray REST client.
//
// Jira Cloud:       GET  {baseUrl}/rest/api/3/search/jql      (Basic auth: email + API token)
// Jira Data Center: GET  {baseUrl}/rest/api/2/search          (Bearer personal access token,
//                                                              or Basic if RG_JIRA_USER is set)
// Xray Cloud:       POST {xrayBaseUrl}/api/v2/authenticate    (client id/secret -> token)
//                   POST {xrayBaseUrl}/api/v2/import/execution
// Xray Server/DC:   POST {baseUrl}/rest/raven/1.0/import/execution
//
// Secrets come from environment variables only:
//   RG_JIRA_USER, RG_JIRA_TOKEN, RG_XRAY_CLIENT_ID, RG_XRAY_CLIENT_SECRET
#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QVector>
#include <functional>

class AppConfig;
class QNetworkReply;

struct JiraTest {
    QString key;
    QString summary;
    QString status;
};

class JiraXrayClient : public QObject {
    Q_OBJECT
public:
    JiraXrayClient(const AppConfig* config, QObject* parent = nullptr);

    bool jiraConfigured(QString* why = nullptr) const;
    bool xrayConfigured(QString* why = nullptr) const;
    bool isCloud() const;

    void searchTests();                              // -> testsReady
    void importExecution(const QByteArray& xrayJson);  // -> importFinished

signals:
    void testsReady(const QVector<JiraTest>& tests, const QString& error);
    void importFinished(bool ok, const QString& message);

private:
    QNetworkRequest jiraRequest(const QUrl& url) const;
    void searchPage(const QString& jql, const QString& pageToken, int startAt, QVector<JiraTest> acc);
    void authenticateXray(std::function<void(const QString& token, const QString& error)> done);
    static QString replyError(QNetworkReply* r);
    bool urlAllowed(const QString& url, QString* why) const;

    const AppConfig* config_;
    QNetworkAccessManager nam_;
};
