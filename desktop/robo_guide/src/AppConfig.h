// Application configuration. Non-secret settings live in JSON; secrets come only from
// environment variables (see config/.env.example) and are never written to disk by the app.
#pragma once

#include <QMap>
#include <QString>
#include <QVector>

struct CameraConfig {
    QString id;               // "cam1"
    QString name;             // "Workspace overview"
    QString role;             // "overview" | "gripper" | "reader" | "inspection" | "arm"
    QString host;             // camera IP
    int port = 554;
    QString stream = "stream2";  // Tapo: stream1 = high quality, stream2 = low quality
    QString url;              // optional full URL override (e.g. ESP32-CAM http://ip:81/stream); no credentials
    bool enabled = false;
};

struct JiraConfig {
    QString deployment = "cloud";  // "cloud" | "datacenter"
    QString baseUrl;               // https://yourcompany.atlassian.net or https://jira.company.local
    QString xrayBaseUrl = "https://xray.cloud.getxray.app";  // cloud only
    QString projectKey;
    QString testJql;               // empty -> "project = KEY AND issuetype = Test"
    QMap<QString, QString> statusMap;  // verdict -> Xray status override, e.g. {"ERROR":"ABORTED"}
    bool allowInsecureHttp = false;
};

struct PlatformConfig {
    QString mode = "none";  // "none" | "simulated"  (real client to be added once the interface is approved)
    bool allowOperatorObservation = true;
};

class AppConfig {
public:
    // Loads, in order of precedence: $RG_CONFIG, the user config, <repo>/config/robo_guide.json,
    // <repo>/config/robo_guide.example.json. Always leaves a usable config in memory.
    bool load(QString* error);
    // Writes non-secret settings to the user config file.
    bool save(QString* error) const;

    static QString userConfigPath();
    static QString repoRoot();  // $RG_HOME or a parent of the executable containing config/
    static QString secret(const char* envName);
    static bool hasSecret(const char* envName);

    QString cameraUrl(int index, bool redacted) const;

    QString loadedFrom;
    QString robotIp = "192.168.178.31";
    QVector<CameraConfig> cameras;
    JiraConfig jira;
    PlatformConfig platform;
    QString evidenceDir;          // empty -> <AppData>/evidence
    QString testDefinitionsDir;   // empty -> <repo>/config/test_definitions
    QString bindingsFile;         // empty -> <repo>/config/test_bindings.json

    QString resolvedEvidenceDir() const;
    QString resolvedDefinitionsDir() const;
    QString resolvedBindingsFile() const;

private:
    bool loadFile(const QString& path, QString* error);
    void ensureCameraSlots();
};
