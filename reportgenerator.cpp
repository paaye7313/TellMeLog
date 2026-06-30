#include "reportgenerator.h"

#include <QWidget>
#include <QPrinter>
#include <QPrintPreviewDialog>
#include <QTextDocument>
#include <QFileInfo>
#include <QDateTime>
#include <QPainter>
#include <QFontMetrics>
#include <QUrl>
#include <QApplication>
#include <QCursor>
#include <algorithm>

// ── 막대그래프 렌더링 (파일 내부 전용) ──────────────────────
// data: (라벨, 값) 쌍 목록 — 이미 정렬/TOP20 컷 된 상태로 전달받음
static QPixmap renderBarChart(const QVector<QPair<QString,int>> &data,
                              const QString &title)
{
    const int W = 700;
    const int barH = 18;
    const int gap = 6;
    const int leftMargin = 220;   // 라벨 영역
    const int rightMargin = 50;   // 값 표시 영역
    const int topMargin = 36;
    const int bottomMargin = 10;
    const int n = data.size();
    const int H = topMargin + bottomMargin + n * (barH + gap);

    QPixmap pix(W, qMax(H, topMargin + bottomMargin + barH));
    pix.fill(Qt::white);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    QFont titleFont = p.font();
    titleFont.setBold(true);
    titleFont.setPointSize(11);
    p.setFont(titleFont);
    p.setPen(QColor("#1a3a5c"));
    p.drawText(QRect(0, 4, W, 24), Qt::AlignLeft | Qt::AlignVCenter, title);

    int maxVal = 1;
    for (const auto &kv : data) maxVal = qMax(maxVal, kv.second);

    const int chartW = W - leftMargin - rightMargin;

    QFont labelFont = p.font();
    labelFont.setBold(false);
    labelFont.setPointSize(8);
    p.setFont(labelFont);
    QFontMetrics fm(labelFont);

    int y = topMargin;
    for (const auto &kv : data) {
        QString label = fm.elidedText(kv.first.isEmpty() ? "(없음)" : kv.first,
                                      Qt::ElideRight, leftMargin - 10);
        p.setPen(QColor("#333"));
        p.drawText(QRect(0, y, leftMargin - 10, barH),
                   Qt::AlignVCenter | Qt::AlignRight, label);

        int barW = static_cast<int>(static_cast<double>(kv.second) / maxVal * chartW);
        p.fillRect(leftMargin, y, qMax(barW, 2), barH, QColor("#1a6cbf"));

        p.setPen(QColor("#222"));
        p.drawText(QRect(leftMargin + barW + 6, y, rightMargin, barH),
                   Qt::AlignVCenter | Qt::AlignLeft, QString::number(kv.second));

        y += barH + gap;
    }

    p.end();
    return pix;
}

// ── HTML 생성 ────────────────────────────────────────────
QString ReportGenerator::buildHtml(const QVector<LogEntry> &entries,
                                   const QString &sourceFile,
                                   QTextDocument &doc)
{
    // ── 통계 계산 ──
    int totalCount = entries.size();
    int errorCount = 0, warnCount = 0, infoCount = 0, noiseCount = 0;
    QDateTime firstDt, lastDt;

    // 모듈별 ERROR/WARN 카운트: QMap<모듈명, {error, warn}>
    QMap<QString, QPair<int,int>> moduleStats;
    // 메시지별 ERROR/WARN 카운트 (완전 동일 문자열 기준)
    QMap<QString, int> messageStats;

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

            // 메시지별 카운트 (완전 동일 문자열 기준)
            messageStats[e.message.trimmed()]++;
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

    // ── 모듈별 오류 빈도 TOP 20 그래프 ──
    html += "<h2>모듈별 오류 빈도 TOP 20</h2>\n";
    if (moduleStats.isEmpty()) {
        html += "<p class='no-issues'>✅ ERROR/WARN 없음</p>\n";
    } else {
        // 합산(ERROR+WARN) 많은 순 정렬
        QVector<QPair<QString, QPair<int,int>>> sortedModules;
        for (auto it = moduleStats.begin(); it != moduleStats.end(); ++it)
            sortedModules.append({it.key(), it.value()});
        std::sort(sortedModules.begin(), sortedModules.end(),
                  [](const auto &a, const auto &b){
                      return (a.second.first + a.second.second)
                      > (b.second.first + b.second.second);
                  });

        QVector<QPair<QString,int>> moduleChartData;
        for (int i = 0; i < sortedModules.size() && i < 20; ++i) {
            moduleChartData.append({sortedModules[i].first,
                                    sortedModules[i].second.first
                                        + sortedModules[i].second.second});
        }

        QPixmap moduleChart = renderBarChart(moduleChartData, "모듈별 ERROR+WARN 건수");
        doc.addResource(QTextDocument::ImageResource, QUrl("chart://module"), moduleChart);
        html += QString("<img src='chart://module' width='%1' height='%2'/>\n")
                    .arg(moduleChart.width()).arg(moduleChart.height());
    }

    // ── 메시지별 발생 빈도 TOP 20 그래프 ──
    html += "<h2>메시지별 발생 빈도 TOP 20</h2>\n";
    if (messageStats.isEmpty()) {
        html += "<p class='no-issues'>✅ ERROR/WARN 없음</p>\n";
    } else {
        QVector<QPair<QString,int>> sortedMsg;
        for (auto it = messageStats.begin(); it != messageStats.end(); ++it)
            sortedMsg.append({it.key(), it.value()});
        std::sort(sortedMsg.begin(), sortedMsg.end(),
                  [](const auto &a, const auto &b){ return a.second > b.second; });

        QVector<QPair<QString,int>> msgChartData;
        for (int i = 0; i < sortedMsg.size() && i < 20; ++i)
            msgChartData.append(sortedMsg[i]);

        QPixmap msgChart = renderBarChart(msgChartData, "메시지 발생 건수");
        doc.addResource(QTextDocument::ImageResource, QUrl("chart://message"), msgChart);
        html += QString("<img src='chart://message' width='%1' height='%2'/>\n")
                    .arg(msgChart.width()).arg(msgChart.height());
    }

    html += "</body></html>";
    return html;
}

// ── 미리보기 + PDF 저장 ──────────────────────────────────
void ReportGenerator::generate(QWidget *parent,
                               const QVector<LogEntry> &entries,
                               const QString &sourceFile)
{
    // 통계 계산 + 차트 렌더링 동안 대기 커서 표시 (체감 응답성 개선)
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // QPrinter 설정 (PDF 출력 대상)
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPageSize::A4);
    printer.setPageOrientation(QPageLayout::Portrait);

    // QTextDocument 먼저 생성 — buildHtml 내부에서 차트 이미지를
    // doc.addResource()로 등록하기 때문에 doc을 참조로 넘겨야 함
    QTextDocument doc;
    QString html = buildHtml(entries, sourceFile, doc);
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

    QApplication::restoreOverrideCursor();

    preview.exec();  // 모달 다이얼로그 (닫을 때까지 블록)
}