#ifndef LOGPARSER_H
#define LOGPARSER_H

#include <QString>
#include <QVector>
#include <QFile>

// 파싱된 로그 한 줄을 담는 구조체
// PyQt였다면 dataclass나 dict로 쓸 부분
struct LogEntry {
    QString date;
    QString timestamp;
    QString level;
    QString module;
    QString message;
    bool    parsed;    // false = 노이즈 라인
};

class LogParser
{
public:
    LogParser() = default;

    // 파일 경로를 받아 파싱 후 결과 반환
    QVector<LogEntry> parse(const QString &filePath);

    // 마지막 파싱에서 실패한 줄 수
    int noiseCount() const { return m_noiseCount; }

private:
    LogEntry parseLine(const QString &line);

    int m_noiseCount = 0;
};

#endif // LOGPARSER_H