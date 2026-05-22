#include "logparser.h"

#include <QTextStream>
#include <QRegularExpression>

QVector<LogEntry> LogParser::parse(const QString &filePath)
{
    QVector<LogEntry> entries;
    m_noiseCount = 0;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return entries;

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;

        LogEntry entry = parseLine(line);
        if (!entry.parsed)
            ++m_noiseCount;

        entries.append(entry);
    }

    file.close();
    return entries;
}

LogEntry LogParser::parseLine(const QString &line)
{
    LogEntry entry;
    entry.parsed = false;
    entry.message = line;

    // 그룹1=날짜, 그룹2=시간, 그룹3=레벨, 그룹4=모듈, 그룹5=메시지
    static const QRegularExpression reStandard(
        R"(^(\d{4}-\d{2}-\d{2})\s+(\d{2}:\d{2}:\d{2}\.\d+)\s+\[(\w+)\]\s+\[([^\]]+)\]\s+(.+)$)"
        );

    static const QRegularExpression reSlash(
        R"(^(\d{2,4}[/]\d{2}[/]\d{2,4})\s+(\d{2}:\d{2}:\d{2}\.\d+)\s+\[(\w+)\]\s+\[([^\]]+)\]\s+(.+)$)"
        );

    static const QRegularExpression reISO(
        R"(^(\d{4}-\d{2}-\d{2})T(\d{2}:\d{2}:\d{2}\.\d+)[^\s]*\s+\[(\w+)\]\s+\[([^\]]+)\]\s+(.+)$)"
        );

    for (const QRegularExpression *re : {&reStandard, &reSlash, &reISO}) {
        QRegularExpressionMatch m = re->match(line);
        if (m.hasMatch()) {
            entry.date      = m.captured(1); // 날짜
            entry.timestamp = m.captured(2); // 시간
            entry.level     = m.captured(3);
            entry.module    = m.captured(4);
            entry.message   = m.captured(5);
            entry.parsed    = true;
            return entry;
        }
    }

    return entry;
}