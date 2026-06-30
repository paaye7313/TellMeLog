#ifndef REPORTGENERATOR_H
#define REPORTGENERATOR_H

#include <QString>
#include <QVector>
#include "logparser.h"

class QWidget;
class QTextDocument;

class ReportGenerator
{
public:
    // parent: 미리보기 다이얼로그의 부모 위젯
    // entries: m_allEntries (전체 파싱 결과)
    // sourceFile: 원본 로그 파일 경로 (리포트 제목용)
    void generate(QWidget *parent,
                  const QVector<LogEntry> &entries,
                  const QString &sourceFile);

private:
    // doc: 차트 이미지를 addResource()로 등록할 QTextDocument (generate()에서 생성해서 전달)
    QString buildHtml(const QVector<LogEntry> &entries,
                      const QString &sourceFile,
                      QTextDocument &doc);
};

#endif // REPORTGENERATOR_H