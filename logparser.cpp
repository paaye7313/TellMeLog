#include "logparser.h"

#include <QTextStream>
#include <QRegularExpression>
#include <QFileInfo>  // ★ 추가

QVector<LogEntry> LogParser::parse(const QString &filePath)
{
    m_noiseCount = 0;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    // ★ 확장자에 따라 파싱 분기
    QFileInfo fi(filePath);
    if (fi.suffix().toLower() == "csv")
        return parseCsv(file);

    // 기존 로그 파싱
    QVector<LogEntry> entries;
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

// ★ CSV 파싱 ─────────────────────────────────────────────
QVector<LogEntry> LogParser::parseCsv(QFile &file)
{
    QVector<LogEntry> entries;
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    // 날짜 형식 판별용 regex
    static const QRegularExpression reDate(
        R"(^\d{4}[-/]\d{2}[-/]\d{2}$|^\d{2}[/]\d{2}[/]\d{4}$|^\d{4}-\d{2}-\d{2}T)"
        );

    // 날짜 컬럼 인덱스 찾는 람다
    // 첫 줄을 받아서 날짜처럼 보이는 컬럼 인덱스 반환, 없으면 -1
    auto findDateColumn = [&](const QStringList &fields) -> int {
        for (int i = 0; i < fields.size(); ++i) {
            if (reDate.match(fields[i].trimmed()).hasMatch())
                return i;
        }
        return -1;
    };

    // 첫 줄 읽기
    QString firstLine = in.readLine();
    if (!firstLine.isEmpty() && firstLine[0] == QChar(0xFEFF))
        firstLine.remove(0, 1);
    firstLine = firstLine.trimmed();

    QStringList firstFields = splitCsvLine(firstLine);
    int dateCol = findDateColumn(firstFields);

    // 첫 줄이 헤더인지 데이터인지 결정
    // 날짜 컬럼 못 찾으면 헤더로 간주하고 다음 줄부터 파싱
    bool firstLineIsHeader = (dateCol == -1);

    if (firstLineIsHeader) {
        // 헤더 줄 스킵 후 첫 데이터 줄로 dateCol 재탐색
        if (in.atEnd()) {
            file.close();
            return entries;
        }
        QString secondLine = in.readLine().trimmed();
        QStringList secondFields = splitCsvLine(secondLine);
        dateCol = findDateColumn(secondFields);

        if (dateCol == -1) {
            // 두 번째 줄도 날짜 없음 → 전부 노이즈
            auto addNoise = [&](const QString &line) {
                LogEntry e; e.parsed = false; e.message = line;
                entries.append(e); ++m_noiseCount;
            };
            addNoise(firstLine);
            addNoise(secondLine);
            while (!in.atEnd()) {
                QString l = in.readLine().trimmed();
                if (!l.isEmpty()) addNoise(l);
            }
            file.close();
            return entries;
        }

        // 두 번째 줄을 첫 데이터로 처리
        firstFields = secondFields;
        firstLineIsHeader = false; // 이제 firstFields는 데이터
    }

    // 데이터 파싱 람다 (dateCol 기준으로 5컬럼 매핑)
    auto parseFields = [&](const QStringList &fields) -> LogEntry {
        LogEntry entry;
        entry.parsed = false;

        if (dateCol >= fields.size()) {
            entry.message = fields.join(',');
            return entry;
        }

        // dateCol 기준: date, date+1=time, date+2=level, date+3=module, date+4~=message
        int timeCol    = dateCol + 1;
        int levelCol   = dateCol + 2;
        int moduleCol  = dateCol + 3;
        int msgCol     = dateCol + 4;

        if (msgCol >= fields.size()) {
            // 컬럼 부족 → 노이즈
            entry.message = fields.join(',');
            return entry;
        }

        entry.date      = fields[dateCol].trimmed();
        entry.timestamp = fields[timeCol].trimmed();
        entry.level     = fields[levelCol].trimmed();
        entry.module    = fields[moduleCol].trimmed();

        // msgCol 이후 초과 컬럼은 메시지에 이어붙임
        QStringList msgParts;
        for (int i = msgCol; i < fields.size(); ++i)
            msgParts.append(fields[i]);
        entry.message = msgParts.join(' ').trimmed();

        // ── 날짜 형식 검증 ──
        static const QRegularExpression reDateValid(
            R"(^\d{4}[-/]\d{2}[-/]\d{2}$|^\d{2}/\d{2}/\d{4}$|^\d{4}-\d{2}-\d{2}T)"
            );
        if (!reDateValid.match(entry.date).hasMatch()) {
            entry.parsed = false;
            entry.message = fields.join(',');
            ++m_noiseCount;
            return entry;
        }

        // ── 레벨 검증 ──
        static const QStringList validLevels = {
            "ERROR", "WARN", "WARNING", "INFO", "DEBUG", "TRACE", "FATAL", "CRITICAL"
        };
        if (!validLevels.contains(entry.level.toUpper())) {
            entry.parsed = false;
            entry.message = fields.join(',');
            ++m_noiseCount;
            return entry;
        }

        entry.parsed = true;
        return entry;
    };

    // 첫 데이터 줄 처리
    {
        LogEntry e = parseFields(firstFields);
        entries.append(e);
    }

    // 나머지 줄 처리
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.trimmed().isEmpty()) continue;

        QStringList fields = splitCsvLine(line);
        LogEntry e = parseFields(fields);
        entries.append(e);
    }

    file.close();
    return entries;
}

// ★ RFC 4180 CSV 한 줄 파싱 ───────────────────────────────
// "필드1","필드,안에,쉼표","따옴표""포함" 모두 처리
QStringList LogParser::splitCsvLine(const QString &line)
{
    QStringList fields;
    QString field;
    bool inQuotes = false;

    for (int i = 0; i < line.size(); ++i) {
        QChar c = line[i];

        if (inQuotes) {
            if (c == '"') {
                // 다음 문자도 '"' 이면 이스케이프된 따옴표
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field += '"';
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field += c;
            }
        } else {
            if (c == '"') {
                inQuotes = true;
            } else if (c == ',') {
                fields.append(field);
                field.clear();
            } else {
                field += c;
            }
        }
    }
    fields.append(field); // 마지막 필드
    return fields;
}

// 기존 로그 라인 파싱 (변경 없음)
LogEntry LogParser::parseLine(const QString &line)
{
    LogEntry entry;
    entry.parsed = false;
    entry.message = line;

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
            entry.date      = m.captured(1);
            entry.timestamp = m.captured(2);
            entry.level     = m.captured(3);
            entry.module    = m.captured(4);
            entry.message   = m.captured(5);
            entry.parsed    = true;
            return entry;
        }
    }

    return entry;
}