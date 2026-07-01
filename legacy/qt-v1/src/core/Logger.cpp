/**
 * @file Logger.cpp
 * @brief 日志系统实现
 *
 * 安装 qInstallMessageHandler，将 Qt 日志分级输出到文件和 stderr。
 * 自动按大小轮转（超过 5MB 重命名备份）。
 */

#include "core/Logger.h"
#include "core/PathManager.h"

#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>
#include <QMutex>
#include <QDateTime>
#include <QDebug>
#include <cstdio>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

static QFile s_logFile;
static QMutex s_mutex;
static const qint64 kMaxLogSize = 5 * 1024 * 1024;  // 5MB
static int s_unflushed = 0;
static const int kFlushInterval = 50;  // 每 50 条日志 flush 一次

static const char* levelString(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:    return "DEBUG";
    case QtInfoMsg:     return "INFO ";
    case QtWarningMsg:  return "WARN ";
    case QtCriticalMsg: return "ERROR";
    case QtFatalMsg:    return "FATAL";
    }
    return "?????";
}

static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QMutexLocker locker(&s_mutex);

    QByteArray timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz").toUtf8();
    QByteArray level = levelString(type);
    QByteArray localMsg = msg.toUtf8();

    // 格式: [时间] [级别] 消息 (文件:行号)
    QByteArray line = QByteArray("[") + timestamp + "] [" + level + "] " + localMsg;
    if (context.file) {
        line += " (" + QByteArray(context.file) + ":" + QByteArray::number(context.line) + ")";
    }
    line += "\n";

    // 输出到 stderr
    fprintf(stderr, "%s", line.constData());

    // 输出到文件
    if (s_logFile.isOpen()) {
        // 轮转检查
        if (s_logFile.size() > kMaxLogSize) {
            QString filePath = s_logFile.fileName();
            s_logFile.close();
            QString backupPath = filePath + ".old";
            QFile::remove(backupPath);
            QFile::rename(filePath, backupPath);
            s_logFile.setFileName(filePath);
            if (!s_logFile.open(QIODevice::Append | QIODevice::Text)) {
                fprintf(stderr, "Dock_WMac: failed to reopen log file after rotation\n");
            }
        }
        s_logFile.write(line);
        if (++s_unflushed >= kFlushInterval) {
            s_logFile.flush();
            s_unflushed = 0;
        }
    }

    if (type == QtFatalMsg) {
        abort();
    }
}

QString Logger::logFilePath()
{
    return PathManager::logFile();
}

QString Logger::logDir()
{
    return PathManager::dataDir();
}

void Logger::init()
{
#ifdef Q_OS_WIN
    // 设置控制台输出代码页为 UTF-8，防止中文乱码
    SetConsoleOutputCP(CP_UTF8);
#endif

    PathManager::ensureDataDir();

    QString path = logFilePath();
    s_logFile.setFileName(path);
    if (!s_logFile.open(QIODevice::Append | QIODevice::Text)) {
        fprintf(stderr, "Dock_WMac: failed to open log file: %s\n",
                path.toUtf8().constData());
    }

    qInstallMessageHandler(messageHandler);

    qInfo() << "=== Dock_WMac 启动 ===";
    qInfo() << "日志文件:" << path;
}
