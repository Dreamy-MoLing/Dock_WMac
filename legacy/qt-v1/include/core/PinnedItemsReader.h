#ifndef PINNEDITEMSREADER_H
#define PINNEDITEMSREADER_H

#include <QObject>
#include <QList>
#include "Types.h"

/**
 * @file PinnedItemsReader.h
 * @brief 任务栏固定项读取器
 *
 * 从 Windows 任务栏 .lnk 文件目录读取固定项，
 * 保留 .lnk 启动目标，并尽量解析目标路径、参数和 AppUserModelID。
 * 自动去重，确保完整读取任务栏固定图标，不重不漏。
 */

class PinnedItemsReader : public QObject {
    Q_OBJECT
public:
    explicit PinnedItemsReader(QObject *parent = nullptr);

    /**
     * @brief 读取所有固定项（从 .lnk 文件目录读取并去重）
     * @return 去重后的固定项列表
     */
    QList<DockItemData> getAllPinnedItems();

    /**
     * @brief 使用 .lnk 文件目录读取
     * @return 固定项列表，失败时返回空列表
     */
    QList<DockItemData> readFromLnkFiles();

private:
    /**
     * @brief 基于身份候选键去重
     * @param items 原始列表
     * @return 去重后的列表
     */
    QList<DockItemData> deduplicateItems(const QList<DockItemData> &items);

    /**
     * @brief 从 .lnk 文件解析目标路径
     * @param lnkPath .lnk 文件路径
     * @return 快捷方式目标路径，失败时返回空
     */
    QString resolveShortcut(const QString &lnkPath);

};

#endif // PINNEDITEMSREADER_H
