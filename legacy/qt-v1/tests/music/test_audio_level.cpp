#include "music/system/AudioLevelSmoothing.h"

#include <gtest/gtest.h>

TEST(AudioLevelTest, RisesQuicklyAndFallsSlowlyWithoutOvershoot)
{
    const qreal rising = music::smoothAudioLevel(0.0, 1.0);
    EXPECT_GT(rising, 0.5);
    EXPECT_LT(rising, 1.0);

    const qreal falling = music::smoothAudioLevel(1.0, 0.0);
    EXPECT_GT(falling, 0.8);
    EXPECT_LT(falling, 1.0);
}

TEST(AudioLevelTest, ClampsInputs)
{
    EXPECT_GE(music::smoothAudioLevel(-5.0, -1.0), 0.0);
    EXPECT_LE(music::smoothAudioLevel(3.0, 2.0), 1.0);
}
