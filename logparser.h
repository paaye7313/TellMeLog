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
    bool    parsed;      // false = 노이즈 라인
    QString sourceFile;  // 출처 파일 경로 (병합 시 색상 구분용)
};

class LogParser
{
public:
    LogParser() = default;

    // 파일 경로를 받아 파싱 후 결과 반환
    QVector<LogEntry> parse(const QString &filePath);

    // ★ 실시간 감시용: startOffset 위치부터 끝까지만 파싱
    //   반환값: 새로 파싱된 엔트리들
    //   outEndOffset: 파싱 후 파일 끝 위치 (다음 호출 시 startOffset으로 사용)
    QVector<LogEntry> parseTail(const QString &filePath,
                                qint64 startOffset,
                                qint64 &outEndOffset);

    // 마지막 파싱에서 실패한 줄 수
    int noiseCount() const { return m_noiseCount; }

private:
    LogEntry parseLine(const QString &line);
    QVector<LogEntry> parseCsv(QFile &file);

    // RFC 4180 필드 하나 파싱 (따옴표 이스케이프 처리)
    QStringList splitCsvLine(const QString &line);

    int m_noiseCount = 0;
};

#endif // LOGPARSER_H