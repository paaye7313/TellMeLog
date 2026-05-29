#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include <QDateTimeEdit>  // ★ 추가
#include <QCheckBox>      // ★ 추가
#include <QComboBox>      // ★ 추가
#include <QDateEdit>
#include <QTimeEdit>
#include <QLineEdit>   // ★ 추가
#include "logparser.h"
#include "reportgenerator.h"

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
    void applyFilters();   // ★ 추가: 필터/정렬 적용

private:
    void setupUI();
    void setupToolBar();
    void setupFilterBar();  // ★ 추가
    void parseAndDisplay(const QString &filePath);
    void populateTable(const QVector<LogEntry> &entries);

    // ── 기존 위젯 ──
    QListWidget  *m_fileListWidget;
    QTableWidget *m_logTableWidget;

    QPushButton  *m_addFileBtn;
    QPushButton  *m_removeFileBtn;
    QPushButton  *m_parseBtn;
    QPushButton  *m_reportBtn;
    QPushButton  *m_csvBtn;

    // ── 필터 바 위젯 ★ 추가 ──
    QCheckBox      *m_chkError;
    QCheckBox      *m_chkWarn;
    QCheckBox      *m_chkInfo;
    QCheckBox      *m_chkNoise;
    QDateEdit  *m_dateFrom;
    QDateEdit  *m_dateTo;
    QTimeEdit  *m_timeFrom;
    QTimeEdit  *m_timeTo;
    QPushButton    *m_btnResetTime;   // 시간 범위 초기화
    QComboBox      *m_sortCombo;      // 정렬 방향
    QLineEdit      *m_searchEdit;     // ★ 검색창
    QComboBox      *m_searchScopeCombo; // ★ 검색 범위

    // ── 데이터 ──
    LogParser          m_parser;
    QString            m_currentFile;
    QVector<LogEntry>  m_allEntries;  // ★ 전체 파싱 결과 보관

    // ── 리포트 ──
    ReportGenerator m_reportGen;
};

#endif // MAINWINDOW_H