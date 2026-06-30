#include "music/ui/NowPlayingPanel.h"

#include <QApplication>
#include <QSignalSpy>
#include <QTest>

#include <gtest/gtest.h>

namespace {

static int argc = 1;
static char arg0[] = "test_now_playing_panel";
static char *argv[] = {arg0, nullptr};

class NowPlayingPanelTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (!QApplication::instance())
            app = new QApplication(argc, argv);
    }

    static void TearDownTestSuite()
    {
        delete app;
        app = nullptr;
    }

    static QApplication *app;
};

QApplication *NowPlayingPanelTest::app = nullptr;

music::NowPlayingSnapshot activeSnapshot()
{
    music::NowPlayingSnapshot snapshot;
    snapshot.available = true;
    snapshot.status = music::PlaybackStatus::Playing;
    snapshot.title = QStringLiteral("Song");
    snapshot.artist = QStringLiteral("Artist");
    snapshot.album = QStringLiteral("Album");
    snapshot.canPrevious = true;
    snapshot.canPlayPause = true;
    snapshot.canNext = true;
    snapshot.audioLevel = 0.4;
    snapshot.hasLyrics = true;
    snapshot.previousLyric = QStringLiteral("Previous lyric");
    snapshot.currentLyric = QStringLiteral("Current lyric");
    snapshot.nextLyric = QStringLiteral("Next lyric");
    return snapshot;
}

} // namespace

TEST_F(NowPlayingPanelTest, ActiveSessionShowsDockPanelState)
{
    music::NowPlayingPanel panel;
    panel.setSnapshot(activeSnapshot());
    QCoreApplication::processEvents();

    EXPECT_TRUE(panel.isPanelActive());
    EXPECT_TRUE(panel.isVisible());
    EXPECT_TRUE(panel.isExpanded());
    EXPECT_GT(panel.effectiveWidth(), 48);
    EXPECT_EQ(panel.currentLyric(), QStringLiteral("Current lyric"));
    EXPECT_DOUBLE_EQ(panel.audioLevel(), 0.4);
}

TEST_F(NowPlayingPanelTest, IconClickTogglesExpandedWidth)
{
    music::NowPlayingPanel panel;
    panel.setSnapshot(activeSnapshot());
    const int expandedWidth = panel.effectiveWidth();

    QTest::mouseClick(&panel, Qt::LeftButton, Qt::NoModifier,
                      QPoint(8, panel.height() - 8));
    QCoreApplication::processEvents();

    EXPECT_FALSE(panel.isExpanded());
    EXPECT_LT(panel.effectiveWidth(), expandedWidth);
}

TEST_F(NowPlayingPanelTest, LeftControlButtonsEmitRequests)
{
    music::NowPlayingPanel panel;
    panel.setSnapshot(activeSnapshot());
    QCoreApplication::processEvents();

    QSignalSpy previousSpy(&panel, &music::NowPlayingPanel::previousRequested);
    QSignalSpy playSpy(&panel, &music::NowPlayingPanel::togglePlayPauseRequested);
    QSignalSpy nextSpy(&panel, &music::NowPlayingPanel::nextRequested);

    QTest::mouseClick(&panel, Qt::LeftButton, Qt::NoModifier, QPoint(72, 25));
    QTest::mouseClick(&panel, Qt::LeftButton, Qt::NoModifier, QPoint(72, 55));
    QTest::mouseClick(&panel, Qt::LeftButton, Qt::NoModifier, QPoint(72, 78));

    EXPECT_EQ(previousSpy.count(), 1);
    EXPECT_EQ(playSpy.count(), 1);
    EXPECT_EQ(nextSpy.count(), 1);
}

TEST_F(NowPlayingPanelTest, UnavailableSessionHidesPanel)
{
    music::NowPlayingPanel panel;
    panel.setSnapshot(activeSnapshot());

    music::NowPlayingSnapshot unavailable;
    unavailable.status = music::PlaybackStatus::Unavailable;
    panel.setSnapshot(unavailable);
    QCoreApplication::processEvents();

    EXPECT_FALSE(panel.isPanelActive());
    EXPECT_FALSE(panel.isVisible());
    EXPECT_EQ(panel.effectiveWidth(), 0);
}
