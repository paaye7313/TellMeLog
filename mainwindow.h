#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include "logparser.h"

// 1MB 이상이면 수동 파싱
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
    void onParseFile();       // 파싱 버튼 (대용량용)
    void onGenerateReport();
    void onFileSelected(QListWidgetItem *item);
    void onExportCsv();

private:
    void setupUI();
    void setupToolBar();
    void parseAndDisplay(const QString &filePath); // 실제 파싱 + 테이블 갱신
    void populateTable(const QVector<LogEntry> &entries);

    QListWidget  *m_fileListWidget;
    QTableWidget *m_logTableWidget;

    QPushButton  *m_addFileBtn;
    QPushButton  *m_removeFileBtn;
    QPushButton  *m_parseBtn;     // 수동 파싱 버튼 (신규)
    QPushButton  *m_reportBtn;
    QPushButton  *m_csvBtn;

    LogParser     m_parser;
    QString       m_currentFile;  // 현재 선택된 파일 경로
};

#endif // MAINWINDOW_H