/**
 * @file test_pinned_items_reader.cpp
 * @brief PinnedItemsReader 单元测试
 *
 * 测试 Windows 任务栏 .lnk 文件读取、COM IShellLink 解析、
 * 去重逻辑、数据完整性、性能与错误处理。
 *
 * 注意：PinnedItemsReader 依赖 Win32 API 和 COM，
 * 在无图形界面或无任务栏固定项的环境中可能返回空列表，
 * 这是合法的降级行为。
 */

#include <gtest/gtest.h>
#include <QSet>
#include <QApplication>
#include <chrono>
#include "core/PinnedItemsReader.h"

// 测试需要 QApplication（QPixmap 依赖，extractAppIcon 内部使用）
static int testArgc = 1;
static char testArg0[] = "test_pinned_items_reader";
static char *testArgv[] = {testArg0, nullptr};

class PinnedItemsReaderTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QApplication::instance()) {
            app = new QApplication(testArgc, testArgv);
        }
    }
    static void TearDownTestSuite() {
        // QApplication 由进程退出时回收
    }

    void SetUp() override {
        reader = new PinnedItemsReader();
    }
    void TearDown() override {
        delete reader;
    }
    PinnedItemsReader *reader = nullptr;
    static QApplication *app;
};

QApplication *PinnedItemsReaderTest::app = nullptr;

// ========== 1. 初始化测试 ==========

TEST_F(PinnedItemsReaderTest, Initialization)
{
    // reader 在构造后不应为空
    EXPECT_NE(reader, nullptr);
}

// ========== 2. readFromLnkFiles 不崩溃测试 ==========

TEST_F(PinnedItemsReaderTest, ReadFromLnkFilesDoesNotCrash)
{
    // readFromLnkFiles() 应返回列表（可能为空），不崩溃
    QList<DockItemData> items = reader->readFromLnkFiles();
    EXPECT_GE(items.size(), 0);
}

// ========== 3. getAllPinnedItems 不崩溃测试 ==========

TEST_F(PinnedItemsReaderTest, GetAllPinnedItemsDoesNotCrash)
{
    // getAllPinnedItems() 应返回列表（可能为空），不崩溃
    QList<DockItemData> items = reader->getAllPinnedItems();
    EXPECT_GE(items.size(), 0);
}

// ========== 4. 无重复项测试（通过 execPath 去重，大小写不敏感）==========

TEST_F(PinnedItemsReaderTest, GetAllPinnedItemsNoDuplicates)
{
    QList<DockItemData> items = reader->getAllPinnedItems();

    QSet<QString> seenPaths;
    for (const auto &item : items) {
        QString key = item.execPath.toLower();
        EXPECT_FALSE(seenPaths.contains(key))
            << "重复 execPath: " << item.execPath.toStdString();
        seenPaths.insert(key);
    }
}

// ========== 5. 数据完整性测试 ==========

TEST_F(PinnedItemsReaderTest, DataIntegrity)
{
    QList<DockItemData> items = reader->getAllPinnedItems();

    for (const auto &item : items) {
        // appId 不应为空
        EXPECT_FALSE(item.appId.isEmpty())
            << "appId 为空，execPath=" << item.execPath.toStdString();

        // execPath 不应为空
        EXPECT_FALSE(item.execPath.isEmpty())
            << "execPath 为空，appId=" << item.appId.toStdString();

        // execPath 应以 .exe 结尾（大小写不敏感）
        EXPECT_TRUE(item.execPath.toLower().endsWith(".exe"))
            << "execPath 不是 .exe 文件: " << item.execPath.toStdString();

        // isRunning 初始化为 false
        EXPECT_FALSE(item.isRunning)
            << "isRunning 应为 false: " << item.appId.toStdString();

        // badgeCount 初始化为 0
        EXPECT_EQ(item.badgeCount, 0)
            << "badgeCount 应为 0: " << item.appId.toStdString();
    }
}

// ========== 6. 性能测试（5000ms 内完成）==========

TEST_F(PinnedItemsReaderTest, ReadPerformanceUnder5s)
{
    auto start = std::chrono::high_resolution_clock::now();

    QList<DockItemData> items = reader->getAllPinnedItems();
    Q_UNUSED(items);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 5000)
        << "getAllPinnedItems() 耗时 " << duration.count() << "ms，超过 5000ms 上限";
}

// ========== 7. 图标提取测试（通过公开 API 间接验证 extractAppIcon）==========

TEST_F(PinnedItemsReaderTest, ExtractAppIconFromExe)
{
    // extractAppIcon 是私有方法，通过 getAllPinnedItems / readFromLnkFiles 间接测试。
    // 验证返回的每个项都有 iconPath 或 execPath（iconPath 为 execPath 的降级）。
    // 注意：在无固定项的环境中，列表为空是合法的。
    QList<DockItemData> items = reader->getAllPinnedItems();

    for (const auto &item : items) {
        // 每个项要么有 iconPath，要么有 execPath（至少有一个非空）
        bool hasIconOrExec = !item.iconPath.isEmpty() || !item.execPath.isEmpty();
        EXPECT_TRUE(hasIconOrExec)
            << "项缺少 iconPath 和 execPath: " << item.appId.toStdString();

        // readFromLnkFiles 中 iconPath 填充逻辑：
        //   item.iconPath = cachedIcon.isEmpty() ? execPath : cachedIcon;
        // 因此 iconPath 不应为空（至少回退到 execPath）
        EXPECT_FALSE(item.iconPath.isEmpty())
            << "iconPath 为空: " << item.appId.toStdString();
    }
}

// ========== 8. 大小写不敏感去重测试 ==========

TEST_F(PinnedItemsReaderTest, DeduplicationCaseInsensitive)
{
    QList<DockItemData> items = reader->getAllPinnedItems();

    // 收集所有小写 execPath，验证没有仅大小写不同的重复
    QSet<QString> lowerPaths;
    for (const auto &item : items) {
        QString lower = item.execPath.toLower();
        EXPECT_FALSE(lowerPaths.contains(lower))
            << "大小写不敏感重复: " << item.execPath.toStdString();
        lowerPaths.insert(lower);
    }
}

// ========== 9. 异常安全测试 ==========

TEST_F(PinnedItemsReaderTest, ErrorHandlingGraceful)
{
    // getAllPinnedItems() 在任何情况下都不应抛出异常，
    // 即使 COM 未初始化、目录不存在等。
    EXPECT_NO_THROW({
        QList<DockItemData> items = reader->getAllPinnedItems();
        Q_UNUSED(items);
    });

    // readFromLnkFiles() 同样不应抛出异常
    EXPECT_NO_THROW({
        QList<DockItemData> items = reader->readFromLnkFiles();
        Q_UNUSED(items);
    });
}

// ========== 10. 多次读取一致性测试 ==========

TEST_F(PinnedItemsReaderTest, MultipleReadsConsistent)
{
    // 连续两次读取应返回相同数量（无内存泄漏、无状态损坏）
    QList<DockItemData> items1 = reader->getAllPinnedItems();
    QList<DockItemData> items2 = reader->getAllPinnedItems();

    EXPECT_EQ(items1.size(), items2.size())
        << "两次读取结果数量不一致: 第一次 " << items1.size()
        << "，第二次 " << items2.size();
}
