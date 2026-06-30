#include "music/lyrics/LrcParser.h"

#include <QRegularExpression>
#include <algorithm>

namespace music {

LyricsDocument LrcParser::parse(const QString &raw)
{
    LyricsDocument doc;
    QVector<QPair<int, QString>> parsedLines;
    int offsetMs = 0;

    static const QRegularExpression metaRe(QStringLiteral(R"(^\[(ar|ti|al|by|offset):(.+)\]$)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression timeRe(QStringLiteral(R"(\[(\d{1,2}):(\d{2})(?:\.(\d{1,3}))?\])"));

    const auto rows = raw.split(QLatin1Char('\n'));
    for (QString row : rows) {
        row = row.trimmed();
        if (row.isEmpty())
            continue;

        const auto meta = metaRe.match(row);
        if (meta.hasMatch()) {
            const QString key = meta.captured(1).toLower();
            const QString value = meta.captured(2).trimmed();
            doc.tags.insert(key, value);
            if (key == QStringLiteral("offset"))
                offsetMs = value.toInt();
            continue;
        }

        QVector<int> timestamps;
        auto matches = timeRe.globalMatch(row);
        while (matches.hasNext()) {
            const auto match = matches.next();
            const int minutes = match.captured(1).toInt();
            const int seconds = match.captured(2).toInt();
            const QString fraction = match.captured(3);
            const int millis = fraction.isEmpty()
                ? 0
                : fraction.leftJustified(3, QLatin1Char('0')).left(3).toInt();
            timestamps.append(minutes * 60000 + seconds * 1000 + millis + offsetMs);
        }

        if (timestamps.isEmpty())
            continue;

        QString text = row;
        text.remove(timeRe);
        text = text.trimmed();
        for (int timestamp : timestamps)
            parsedLines.append({timestamp, text});
    }

    std::sort(parsedLines.begin(), parsedLines.end(),
        [](const auto &a, const auto &b) { return a.first < b.first; });

    for (int i = 0; i < parsedLines.size(); ++i) {
        const int start = parsedLines[i].first;
        const int end = (i + 1 < parsedLines.size())
            ? parsedLines[i + 1].first - 1
            : start + 5000;
        doc.lines.append({start, end, parsedLines[i].second});
    }

    doc.synced = !doc.lines.isEmpty();
    return doc;
}

const LrcLine *LrcParser::findCurrentLine(const LyricsDocument &doc, int positionMs)
{
    for (const auto &line : doc.lines) {
        if (positionMs >= line.startMs && positionMs <= line.endMs)
            return &line;
    }
    return nullptr;
}

} // namespace music
