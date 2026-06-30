#ifndef LRCPARSER_H
#define LRCPARSER_H

#include <QMap>
#include <QString>
#include <QVector>

namespace music {

struct LrcLine {
    int startMs = 0;
    int endMs = 0;
    QString text;
};

struct LyricsDocument {
    QMap<QString, QString> tags;
    QVector<LrcLine> lines;
    bool synced = false;
};

class LrcParser {
public:
    static LyricsDocument parse(const QString &raw);
    static const LrcLine *findCurrentLine(const LyricsDocument &doc, int positionMs);
};

} // namespace music

#endif // LRCPARSER_H
