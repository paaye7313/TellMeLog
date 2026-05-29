#include "reportgenerator.h"

#include <QWidget>
#include <QPrinter>
#include <QPrintPreviewDialog>
#include <QTextDocument>
#include <QFileInfo>
#include <QDateTime>

// ── HTML 생성 ────────────────────────────────────────────
QString ReportGenerator::buildHtml(const QVector<LogEntry> &entries,
                                   const QString &sourceFile)
{
    // ── 통계 계산 ──
    int totalCount = entries.size();
    int errorCount = 0, warnCount = 0, infoCount = 0, noiseCount = 0;
    QDateTime firstDt, lastDt;

    // 모듈별 ERROR/WARN 카운트: QMap<모듈명, {error, warn}>
    QMap<QString, QPair<int,int>> moduleStats;

    for (const LogEntry &e : entries) {
        if (!e.parsed) { ++noiseCount; continue; }

        if      (e.level == "ERROR") ++errorCount;
        else if (e.level == "WARN")  ++warnCount;
        else                         ++infoCount;

        // 시간 범위
        QString dtStr = e.date + " " + e.timestamp;
        QDateTime dt = QDateTime::fromString(dtStr, "yyyy-MM-dd HH:mm:ss.zzz");
        if (!dt.isValid())
            dt = QDateTime::fromString(dtStr, "yyyy-MM-dd HH:mm:ss");
        if (dt.isValid()) {
            if (!firstDt.isValid() || dt < firstDt) firstDt = dt;
            if (!lastDt.isValid()  || dt > lastDt)  lastDt  = dt;
        }

        // 모듈 통계 (ERROR/WARN만)
        if (e.level == "ERROR" || e.level == "WARN") {
            if (!moduleStats.contains(e.module))
                moduleStats[e.module] = {0, 0};
            if (e.level == "ERROR") moduleStats[e.module].first++;
            else                    moduleStats[e.module].second++;
        }
    }

    // ── HTML 조립 ──
    QString html;
    html.reserve(64 * 1024);

    // ── 스타일 ──
    html += R"(
<html><head><meta charset="UTF-8">
<style>
  body   { font-family: 'Malgun Gothic', Arial, sans-serif;
           font-size: 10pt; color: #222; margin: 20px; }
  h1     { font-size: 16pt; color: #1a3a5c; border-bottom: 2px solid #1a3a5c;
           padding-bottom: 6px; }
  h2     { font-size: 12pt; color: #1a3a5c; margin-top: 20px;
           border-left: 4px solid #1a6cbf; padding-left: 8px; }
  table  { border-collapse: collapse; width: 100%; margin-top: 8px; }
  th     { background-color: #1a6cbf; color: white; padding: 6px 8px;
           text-align: left; font-size: 9pt; }
  td     { padding: 5px 8px; border-bottom: 1px solid #ddd;
           font-size: 9pt; vertical-align: top; word-break: break-all; }
  tr.error { background-color: #ffe8e8; }
  tr.warn  { background-color: #fff8e0; }
  tr:hover { background-color: #f0f4ff; }
  .badge-error { color: #c0392b; font-weight: bold; }
  .badge-warn  { color: #e67e22; font-weight: bold; }
  .badge-info  { color: #2980b9; }
  .badge-noise { color: #888; }
  .summary-grid { display: table; width: auto; }
  .summary-row  { display: table-row; }
  .summary-cell { display: table-cell; padding: 4px 16px 4px 0; }
  .meta { color: #666; font-size: 9pt; margin-bottom: 4px; }
  .no-issues { color: #27ae60; font-style: italic; }
</style>
</head><body>
)";

    // ── 제목 ──
    QFileInfo fi(sourceFile);
    html += "<h1>📊 TellMeLog 분석 리포트</h1>\n";
    html += QString("<p class='meta'>파일: <b>%1</b></p>\n").arg(fi.fileName());
    html += QString("<p class='meta'>생성 일시: %1</p>\n")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    if (firstDt.isValid() && lastDt.isValid()) {
        html += QString("<p class='meta'>로그 범위: %1 ~ %2</p>\n")
                    .arg(firstDt.toString("yyyy-MM-dd HH:mm:ss"))
                    .arg(lastDt.toString("yyyy-MM-dd HH:mm:ss"));
    }

    // ── 요약 ──
    html += "<h2>요약</h2>\n";
    html += "<table style='width:auto'>\n";
    html += "<tr><th>항목</th><th>건수</th></tr>\n";
    html += QString("<tr><td>전체</td><td>%1</td></tr>\n").arg(totalCount);
    html += QString("<tr class='error'><td class='badge-error'>ERROR</td>"
                    "<td><b>%1</b></td></tr>\n").arg(errorCount);
    html += QString("<tr class='warn'><td class='badge-warn'>WARN</td>"
                    "<td><b>%1</b></td></tr>\n").arg(warnCount);
    html += QString("<tr><td class='badge-info'>INFO / 기타</td>"
                    "<td>%1</td></tr>\n").arg(infoCount);
    html += QString("<tr><td class='badge-noise'>노이즈</td>"
                    "<td>%1</td></tr>\n").arg(noiseCount);
    html += "</table>\n";

    // ── 모듈별 통계 (ERROR/WARN 있는 모듈만) ──
    if (!moduleStats.isEmpty()) {
        html += "<h2>모듈별 오류 통계</h2>\n";
        html += "<table>\n";
        html += "<tr><th>모듈</th><th>ERROR</th><th>WARN</th></tr>\n";

        // ERROR 많은 순으로 정렬
        QVector<QPair<QString, QPair<int,int>>> sorted;
        for (auto it = moduleStats.begin(); it != moduleStats.end(); ++it)
            sorted.append({it.key(), it.value()});
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto &a, const auto &b){
                      return (a.second.first + a.second.second)
                      > (b.second.first + b.second.second);
                  });

        for (const auto &kv : sorted) {
            html += QString("<tr><td>%1</td>"
                            "<td class='badge-error'><b>%2</b></td>"
                            "<td class='badge-warn'>%3</td></tr>\n")
                        .arg(kv.first.isEmpty() ? "(없음)" : kv.first)
                        .arg(kv.second.first)
                        .arg(kv.second.second);
        }
        html += "</table>\n";
    }

    // ── ERROR 목록 ──
    html += "<h2>ERROR 목록</h2>\n";
    if (errorCount == 0) {
        html += "<p class='no-issues'>✅ ERROR 없음</p>\n";
    } else {
        html += "<table>\n";
        html += "<tr><th>날짜</th><th>시간</th><th>모듈</th><th>메시지</th></tr>\n";
        for (const LogEntry &e : entries) {
            if (!e.parsed || e.level != "ERROR") continue;
            // HTML 이스케이프 (꺾쇠 등이 로그에 포함될 수 있음)
            QString msg = e.message;
            msg.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;");
            html += QString("<tr class='error'><td>%1</td><td>%2</td>"
                            "<td>%3</td><td>%4</td></tr>\n")
                        .arg(e.date).arg(e.timestamp)
                        .arg(e.module.isEmpty() ? "-" : e.module)
                        .arg(msg);
        }
        html += "</table>\n";
    }

    // ── WARN 목록 ──
    html += "<h2>WARN 목록</h2>\n";
    if (warnCount == 0) {
        html += "<p class='no-issues'>✅ WARN 없음</p>\n";
    } else {
        html += "<table>\n";
        html += "<tr><th>날짜</th><th>시간</th><th>모듈</th><th>메시지</th></tr>\n";
        for (const LogEntry &e : entries) {
            if (!e.parsed || e.level != "WARN") continue;
            QString msg = e.message;
            msg.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;");
            html += QString("<tr class='warn'><td>%1</td><td>%2</td>"
                            "<td>%3</td><td>%4</td></tr>\n")
                        .arg(e.date).arg(e.timestamp)
                        .arg(e.module.isEmpty() ? "-" : e.module)
                        .arg(msg);
        }
        html += "</table>\n";
    }

    html += "</body></html>";
    return html;
}

// ── 미리보기 + PDF 저장 ──────────────────────────────────
void ReportGenerator::generate(QWidget *parent,
                               const QVector<LogEntry> &entries,
                               const QString &sourceFile)
{
    QString html = buildHtml(entries, sourceFile);

    // QPrinter 설정 (PDF 출력 대상)
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPageSize::A4);
    printer.setPageOrientation(QPageLayout::Portrait);

    // QTextDocument에 HTML 세팅
    QTextDocument doc;
    doc.setHtml(html);
    // pageRect 대신 실제 용지 크기(mm → pt 변환)를 직접 지정
    // A4 가로(Landscape): 297mm, 여백 각 15mm → 텍스트 너비 = 267mm
    // 1mm = 2.8346pt 기준
    doc.setTextWidth(267 * 2.8346);

    // 미리보기 다이얼로그
    // paint 시그널: 미리보기가 페이지를 그릴 때마다 doc.print() 호출
    QPrintPreviewDialog preview(&printer, parent);
    preview.setWindowTitle("리포트 미리보기 — " + QFileInfo(sourceFile).fileName());
    preview.resize(1000, 750);

    QObject::connect(&preview, &QPrintPreviewDialog::paintRequested,
                     [&doc](QPrinter *p) {
                         doc.print(p);
                     });

    preview.exec();  // 모달 다이얼로그 (닫을 때까지 블록)
}