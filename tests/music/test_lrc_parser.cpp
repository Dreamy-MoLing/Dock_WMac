#include "music/lyrics/LrcParser.h"

#include <gtest/gtest.h>

using music::LrcParser;

TEST(LrcParserTest, ParsesTagsMultipleTimestampsAndOffsets)
{
    const QString raw = QStringLiteral(
        "[ar:Artist]\n"
        "[ti:Song]\n"
        "[offset:100]\n"
        "[00:01.50][00:03.000]Line one\n"
        "[00:05.25]Line two\n");

    const auto doc = LrcParser::parse(raw);

    ASSERT_TRUE(doc.synced);
    EXPECT_EQ(doc.tags.value(QStringLiteral("ar")), QStringLiteral("Artist"));
    EXPECT_EQ(doc.tags.value(QStringLiteral("ti")), QStringLiteral("Song"));
    ASSERT_EQ(doc.lines.size(), 3);
    EXPECT_EQ(doc.lines[0].startMs, 1600);
    EXPECT_EQ(doc.lines[0].endMs, 3099);
    EXPECT_EQ(doc.lines[1].startMs, 3100);
    EXPECT_EQ(doc.lines[2].startMs, 5350);
    EXPECT_EQ(doc.lines[2].text, QStringLiteral("Line two"));
}

TEST(LrcParserTest, FindsCurrentLine)
{
    const auto doc = LrcParser::parse(QStringLiteral(
        "[00:01.000]First\n"
        "[00:02.000]Second\n"));

    const auto *line = LrcParser::findCurrentLine(doc, 1500);
    ASSERT_NE(line, nullptr);
    EXPECT_EQ(line->text, QStringLiteral("First"));

    EXPECT_EQ(LrcParser::findCurrentLine(doc, 500), nullptr);
}
