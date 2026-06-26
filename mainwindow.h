#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include <QDateTimeEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QTimeEdit>
#include <QLineEdit>
#include <QProgressBar>
#include <QMap>
#include <QSet>           // ★ 감시 파일 집합
#include <QFileSystemWatcher> // ★ 실시간 감시
#include <QTimer>         // ★ 하이라이트 페이드 타이머
#include <QElapsedTimer>
#include <QFutureWatcher>
#include "logparser.h"
#include "reportgenerator.h"


struct ParseResult {
    QVector<LogEntry> entries;
    QDateTime minDt;
    QDateTime maxDt;
    int noiseCount = 0;
};

static constexpr qint64 AUTO_PARSE_LIMIT = 1 * 1024 * 1024;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onAddFile();
    void onRemoveFile();
    void onParseFile();
    void onGenerateReport();
    void onFileSelected(QListWidgetItem *item);
    void onExportCsv();
    void applyFilters();

    // ★ 실시간 감시 슬롯
    void onToggleWatch();                      // 감시 버튼 토글
    void onFileChanged(const QString &path);   // QFileSystemWatcher 시그널 수신

private:
    void setupUI();
    void setupToolBar();
    void setupFilterBar();
    void parseAndDisplay(const QString &filePath);
    void mergeAndDisplay(const QStringList &filePaths);
    void populateTable(const QVector<LogEntry> &entries);
    bool eventFilter(QObject *obj, QEvent *event) override;

    // ★ 감시 헬퍼
    void startWatching(const QString &filePath);
    void stopWatching(const QString &filePath);
    void stopAllWatching();
    void updateWatchUI();                      // 버튼 텍스트·상태바 갱신
    void appendNewEntries(const QString &filePath,
                          const QVector<LogEntry> &newEntries); // 새 항목 테이블에 추가
    void highlightNewRows(int fromRow, int toRow); // 신규 행 노란 하이라이트

    // ── 기존 위젯 ──
    QListWidget  *m_fileListWidget;
    QTableWidget *m_logTableWidget;

    QPushButton  *m_addFileBtn;
    QPushButton  *m_removeFileBtn;
    QPushButton  *m_parseBtn;
    QPushButton  *m_reportBtn;
    QPushButton  *m_csvBtn;
    QPushButton  *m_watchBtn;   // ★ 감시 토글 버튼

    QToolBar *m_toolBar = nullptr;
    QAction *m_parseBtnAction = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QElapsedTimer m_parseTimer;

    // ── 필터 바 위젯 ──
    QCheckBox      *m_chkError;
    QCheckBox      *m_chkWarn;
    QCheckBox      *m_chkInfo;
    QCheckBox      *m_chkNoise;
    QDateEdit      *m_dateFrom;
    QDateEdit      *m_dateTo;
    QTimeEdit      *m_timeFrom;
    QTimeEdit      *m_timeTo;
    QPushButton    *m_btnResetTime;
    QComboBox      *m_sortCombo;
    QLineEdit      *m_searchEdit;
    QComboBox      *m_searchScopeCombo;

    // ── 데이터 ──
    LogParser          m_parser;
    QString            m_currentFile;
    QVector<LogEntry>  m_allEntries;

    QMap<QString, ParseResult> m_entryCache;

    // ── 병합 색상 ──
    QMap<QString, QColor> m_fileColors;

    // ── 리포트 ──
    ReportGenerator m_reportGen;

    // ── 실시간 감시 ★ ──
    QFileSystemWatcher        *m_watcher;               // Qt 파일 감시자
    QSet<QString>              m_watchedFiles;           // 현재 감시 중인 파일 경로들
    QMap<QString, qint64>      m_fileTailPos;            // 파일별 마지막 읽기 offset
    QTimer                    *m_highlightTimer;         // 하이라이트 페이드 타이머
    QVector<QPair<int,int>>    m_pendingHighlightRanges; // 페이드 대상 행 범위 목록
    int                        m_highlightStep = 0;      // 페이드 단계 (0~FADE_STEPS)

    // 하이라이트 페이드 설정
    static constexpr int FADE_STEPS    = 10;   // 페이드 단계 수
    static constexpr int FADE_INTERVAL = 200;  // 단계 간격 ms (총 2초)
};

#endif // MAINWINDOW_H