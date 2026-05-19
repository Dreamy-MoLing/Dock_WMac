/**
 * @file IPCHelper.cpp
 * @brief Python 子进程 IPC 通信实现
 *
 * 通过 QProcess 管理 Python helper.py 生命周期。
 * stdin/stdout JSON 协议，阻塞式请求-响应。
 */

#include "core/IPCHelper.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QDir>

IPCHelper::IPCHelper(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
}

IPCHelper::~IPCHelper()
{
    stop();
}

bool IPCHelper::start(const QString &scriptPath)
{
    if (m_process->state() != QProcess::NotRunning) {
        return true;  // 已在运行
    }

    // 检查脚本是否存在
    if (!QFileInfo::exists(scriptPath)) {
        qWarning() << "IPCHelper: 脚本不存在:" << scriptPath;
        emit errorOccurred("脚本不存在: " + scriptPath);
        return false;
    }

    // 启动 Python 进程
    m_process->setProgram("python3");
    m_process->setArguments({scriptPath});
    m_process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    m_process->start(QIODevice::ReadWrite);

    if (!m_process->waitForStarted(3000)) {
        qWarning() << "IPCHelper: 启动失败";
        emit errorOccurred("Python 子进程启动失败");
        return false;
    }

    // 读取 ready 信号
    if (!m_process->waitForReadyRead(3000)) {
        qWarning() << "IPCHelper: 未收到 ready 信号";
        stop();
        return false;
    }

    QByteArray line = m_process->readLine().trimmed();
    QJsonDocument doc = QJsonDocument::fromJson(line);
    if (!doc.isObject() || doc.object().value("event").toString() != "ready") {
        qWarning() << "IPCHelper: 收到异常就绪信号:" << line;
        stop();
        return false;
    }

    qInfo() << "IPCHelper: Python helper 已就绪";
    emit ready();
    return true;
}

void IPCHelper::stop()
{
    if (m_process->state() == QProcess::NotRunning) return;

    m_process->terminate();
    if (!m_process->waitForFinished(3000)) {
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}

bool IPCHelper::isRunning() const
{
    return m_process->state() == QProcess::Running;
}

QJsonObject IPCHelper::sendCommand(const QString &cmd, const QJsonObject &params)
{
    if (m_process->state() != QProcess::Running) {
        qWarning() << "IPCHelper: 子进程未运行";
        return {{"status", "error"}, {"message", "子进程未运行"}};
    }

    // 构建请求 JSON
    QJsonObject request;
    request["cmd"] = cmd;
    request["params"] = params;

    QByteArray requestData = QJsonDocument(request).toJson(QJsonDocument::Compact) + "\n";

    // 发送请求
    m_process->write(requestData);
    m_process->waitForBytesWritten(1000);

    // 等待响应
    if (!m_process->waitForReadyRead(kTimeoutMs)) {
        qWarning() << "IPCHelper: 命令" << cmd << "响应超时";
        return {{"status", "error"}, {"message", "响应超时"}};
    }

    QByteArray responseLine = m_process->readLine().trimmed();
    QJsonDocument responseDoc = QJsonDocument::fromJson(responseLine);

    if (!responseDoc.isObject()) {
        qWarning() << "IPCHelper: 无效的 JSON 响应:" << responseLine;
        return {{"status", "error"}, {"message", "无效的 JSON 响应"}};
    }

    return responseDoc.object();
}

// ─── 便捷方法 ─────────────────────────────────────────────

QJsonObject IPCHelper::checkProcesses(const QStringList &names)
{
    QJsonObject params;
    QJsonArray arr;
    for (const auto &name : names) arr.append(name);
    params["names"] = arr;
    return sendCommand("check_processes", params);
}

QJsonObject IPCHelper::parseDesktop(const QString &filePath)
{
    QJsonObject params;
    params["path"] = filePath;
    return sendCommand("parse_desktop", params);
}

QJsonObject IPCHelper::scanDesktopFiles()
{
    return sendCommand("scan_desktop_files");
}

QJsonObject IPCHelper::readConfig()
{
    return sendCommand("read_config");
}

bool IPCHelper::writeConfig(const QJsonObject &data)
{
    QJsonObject params;
    params["data"] = data;
    QJsonObject resp = sendCommand("write_config", params);
    return resp.value("status").toString() == "ok";
}

QJsonObject IPCHelper::scanRunningApps()
{
    return sendCommand("scan_running_apps");
}
