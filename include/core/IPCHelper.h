#ifndef IPCHELPER_H
#define IPCHELPER_H

/**
 * @file IPCHelper.h
 * @brief Python 子进程 IPC 通信封装
 *
 * 通过 stdin/stdout JSON 协议与 scripts/helper.py 通信。
 * 请求-响应模式：C++ 写入一行 JSON → Python 处理 → 返回一行 JSON。
 */

#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

class IPCHelper : public QObject {
    Q_OBJECT
public:
    explicit IPCHelper(QObject *parent = nullptr);
    ~IPCHelper();

    /** @brief 启动 Python 子进程 */
    bool start(const QString &scriptPath);

    /** @brief 停止子进程 */
    void stop();

    /** @brief 发送命令并等待响应（阻塞，但超时保护） */
    QJsonObject sendCommand(const QString &cmd, const QJsonObject &params = {});

    // ─── 便捷方法 ─────────────────────────────────────────

    /** @brief 批量检测进程运行状态 */
    QJsonObject checkProcesses(const QStringList &names);

    /** @brief 解析单个 .desktop 文件 */
    QJsonObject parseDesktop(const QString &filePath);

    /** @brief 扫描所有 .desktop 文件，返回固定项列表 */
    QJsonObject scanDesktopFiles();

    /** @brief 读取配置文件 */
    QJsonObject readConfig();

    /** @brief 写入配置文件 */
    bool writeConfig(const QJsonObject &data);

    /** @brief 扫描所有运行中的应用 */
    QJsonObject scanRunningApps();

    /** @brief 检查子进程是否存活 */
    bool isRunning() const;

signals:
    void ready();
    void errorOccurred(const QString &message);

private:
    QProcess *m_process;
    static const int kTimeoutMs = 5000;  // 5 秒超时
};

#endif // IPCHELPER_H
