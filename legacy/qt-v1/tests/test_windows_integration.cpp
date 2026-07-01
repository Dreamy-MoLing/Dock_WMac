#include <gtest/gtest.h>

#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

#include "core/SysHelper.h"

TEST(WindowsIntegration, RealQtWindowSupportsDwmQueries)
{
    QWidget window;
    window.resize(320, 120);
    window.show();
    QApplication::processEvents();

    const HWND hwnd = reinterpret_cast<HWND>(window.winId());
    ASSERT_NE(hwnd, nullptr);
    ASSERT_TRUE(IsWindow(hwnd));

    SysHelper helper;
    RECT bounds{};
    helper.getExtendedFrameBounds(hwnd, bounds);
    EXPECT_GT(bounds.right, bounds.left);
    EXPECT_GT(bounds.bottom, bounds.top);
    EXPECT_FALSE(SysHelper::isWindowCloaked(hwnd));

    DWORD affinity = WDA_NONE;
    EXPECT_TRUE(SysHelper::getWindowDisplayAffinity(hwnd, affinity));
}

TEST(WindowsIntegration, ScreensExposeValidGeometryAndDpi)
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    ASSERT_FALSE(screens.isEmpty());
    for (const QScreen *screen : screens) {
        ASSERT_NE(screen, nullptr);
        EXPECT_TRUE(screen->geometry().isValid());
        EXPECT_GT(screen->devicePixelRatio(), 0.0);
        EXPECT_GT(screen->logicalDotsPerInch(), 0.0);
    }
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Dock_WMac_IntegrationTests"));
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
