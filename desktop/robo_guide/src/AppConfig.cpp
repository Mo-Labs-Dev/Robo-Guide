#include "AppConfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include "robo_guide/core/util.hpp"

QString AppConfig::userConfigPath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/robo_guide.json";
}

QString AppConfig::repoRoot() {
    const QString env = qEnvironmentVariable("RG_HOME");
    if (!env.isEmpty() && QFileInfo::exists(env + "/config")) return QDir(env).absolutePath();
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        if (QFileInfo::exists(dir.filePath("config/robo_guide.example.json"))) return dir.absolutePath();
        if (!dir.cdUp()) break;
    }
    return QDir::currentPath();
}

QString AppConfig::secret(const char* envName) { return qEnvironmentVariable(envName); }
bool AppConfig::hasSecret(const char* envName) { return !qEnvironmentVariable(envName).isEmpty(); }

void AppConfig::ensureCameraSlots() {
    static const char* roles[] = {"overview", "gripper", "reader", "inspection"};
    static const char* names[] = {"Robot & workspace overview", "Card pickup & gripper", "Access-control reader",
                                  "Result & device inspection"};
    for (int i = cameras.size(); i < 4; ++i) {
        CameraConfig c;
        c.id = QString("cam%1").arg(i + 1);
        c.name = names[i];
        c.role = roles[i];
        cameras.push_back(c);
    }
    for (int i = 0; i < cameras.size(); ++i)
        if (cameras[i].id.isEmpty()) cameras[i].id = QString("cam%1").arg(i + 1);
}

bool AppConfig::load(QString* error) {
    QStringList candidates;
    const QString envPath = qEnvironmentVariable("RG_CONFIG");
    if (!envPath.isEmpty()) candidates << envPath;
    candidates << userConfigPath() << repoRoot() + "/config/robo_guide.json"
               << repoRoot() + "/config/robo_guide.example.json";
    for (const auto& path : candidates) {
        if (!QFileInfo::exists(path)) continue;
        QString err;
        if (loadFile(path, &err)) {
            loadedFrom = path;
            ensureCameraSlots();
            return true;
        }
        if (error) *error = err;
        // A broken explicit config is an error; fall through to defaults so the app still opens.
        break;
    }
    ensureCameraSlots();
    loadedFrom = "(built-in defaults)";
    return error == nullptr || error->isEmpty();
}

bool AppConfig::loadFile(const QString& path, QString* error) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = "Cannot read " + path;
        return false;
    }
    QJsonParseError pe;
    const auto doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (!doc.isObject()) {
        if (error) *error = QString("%1: %2").arg(path, pe.errorString());
        return false;
    }
    const auto root = doc.object();

    const auto robot = root.value("robot").toObject();
    robotIp = robot.value("ip").toString(robotIp);

    cameras.clear();
    for (const auto& v : root.value("cameras").toArray()) {
        const auto o = v.toObject();
        CameraConfig c;
        c.id = o.value("id").toString();
        c.name = o.value("name").toString();
        c.role = o.value("role").toString();
        c.host = o.value("host").toString();
        c.port = o.value("port").toInt(554);
        c.stream = o.value("stream").toString("stream2");
        c.url = o.value("url").toString();
        c.enabled = o.value("enabled").toBool(false);
        cameras.push_back(c);
    }

    const auto j = root.value("jira").toObject();
    jira.deployment = j.value("deployment").toString("cloud");
    jira.baseUrl = j.value("baseUrl").toString();
    jira.xrayBaseUrl = j.value("xrayBaseUrl").toString("https://xray.cloud.getxray.app");
    jira.projectKey = j.value("projectKey").toString();
    jira.testJql = j.value("testJql").toString();
    jira.allowInsecureHttp = j.value("allowInsecureHttp").toBool(false);
    jira.statusMap.clear();
    const auto sm = j.value("statusMap").toObject();
    for (auto it = sm.begin(); it != sm.end(); ++it) jira.statusMap.insert(it.key(), it.value().toString());

    const auto p = root.value("platform").toObject();
    platform.mode = p.value("mode").toString("none");
    platform.allowOperatorObservation = p.value("allowOperatorObservation").toBool(true);

    const auto paths = root.value("paths").toObject();
    evidenceDir = paths.value("evidenceDir").toString();
    testDefinitionsDir = paths.value("testDefinitionsDir").toString();
    bindingsFile = paths.value("bindingsFile").toString();
    return true;
}

bool AppConfig::save(QString* error) const {
    QJsonObject root;
    root["robot"] = QJsonObject{{"ip", robotIp}};
    QJsonArray cams;
    for (const auto& c : cameras) {
        cams.append(QJsonObject{{"id", c.id},
                                {"name", c.name},
                                {"role", c.role},
                                {"host", c.host},
                                {"port", c.port},
                                {"stream", c.stream},
                                {"url", c.url},
                                {"enabled", c.enabled}});
    }
    root["cameras"] = cams;
    QJsonObject sm;
    for (auto it = jira.statusMap.begin(); it != jira.statusMap.end(); ++it) sm[it.key()] = it.value();
    root["jira"] = QJsonObject{{"deployment", jira.deployment},
                               {"baseUrl", jira.baseUrl},
                               {"xrayBaseUrl", jira.xrayBaseUrl},
                               {"projectKey", jira.projectKey},
                               {"testJql", jira.testJql},
                               {"allowInsecureHttp", jira.allowInsecureHttp},
                               {"statusMap", sm}};
    root["platform"] = QJsonObject{{"mode", platform.mode},
                                   {"allowOperatorObservation", platform.allowOperatorObservation}};
    root["paths"] = QJsonObject{{"evidenceDir", evidenceDir},
                                {"testDefinitionsDir", testDefinitionsDir},
                                {"bindingsFile", bindingsFile}};

    const QString path = userConfigPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = "Cannot write " + path;
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        if (error) *error = "Cannot commit " + path;
        return false;
    }
    return true;
}

QString AppConfig::cameraUrl(int index, bool redacted) const {
    if (index < 0 || index >= cameras.size()) return {};
    const auto& c = cameras[index];
    if (!c.url.isEmpty()) return c.url;
    if (c.host.isEmpty()) return {};

    // Per-camera credentials override the shared camera account: RG_CAM1_USER / RG_CAM1_PASSWORD.
    const QByteArray n = QByteArray::number(index + 1);
    QString user = qEnvironmentVariable(("RG_CAM" + n + "_USER").constData());
    QString pass = qEnvironmentVariable(("RG_CAM" + n + "_PASSWORD").constData());
    if (user.isEmpty()) user = qEnvironmentVariable("RG_CAMERA_USER");
    if (pass.isEmpty()) pass = qEnvironmentVariable("RG_CAMERA_PASSWORD");

    QString auth;
    if (!user.isEmpty()) {
        auth = QString::fromStdString(rg::core::encodeUserInfo(user.toStdString()));
        if (!pass.isEmpty())
            auth += ":" + (redacted ? QString("***")
                                    : QString::fromStdString(rg::core::encodeUserInfo(pass.toStdString())));
        auth += "@";
    }
    QString stream = c.stream;
    while (stream.startsWith('/')) stream.remove(0, 1);
    // Plain concatenation on purpose: the encoded password contains '%XX' sequences that
    // QString::arg() would treat as placeholders.
    return "rtsp://" + auth + c.host + ":" + QString::number(c.port) + "/" + stream;
}

static QString resolveRelative(const QString& p) {
    if (p.isEmpty() || QDir::isAbsolutePath(p)) return p;
    return QDir(AppConfig::repoRoot()).filePath(p);
}

QString AppConfig::resolvedEvidenceDir() const {
    if (!evidenceDir.isEmpty()) return resolveRelative(evidenceDir);
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/evidence";
}

QString AppConfig::resolvedDefinitionsDir() const {
    return testDefinitionsDir.isEmpty() ? repoRoot() + "/config/test_definitions" : resolveRelative(testDefinitionsDir);
}

QString AppConfig::resolvedBindingsFile() const {
    return bindingsFile.isEmpty() ? repoRoot() + "/config/test_bindings.json" : resolveRelative(bindingsFile);
}
