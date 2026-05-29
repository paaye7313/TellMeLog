#include "mainwindow.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QSplitter>
#include <QLabel>
#include <QHeaderView>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QToolBar>
#include <QTableWidgetItem>
#include <QColor>
#include <QStatusBar>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFrame>
#include <QLineEdit>   // ★ 추가

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("TellMeLog - 로그야 뭐라고");
    resize(1200, 700);
    setupUI();
    setupToolBar();
    statusBar()->showMessage("파일을 추가하세요.");
}

MainWindow::~MainWindow() {}

// ── UI 전체 구성 ─────────────────────────────────────────
void MainWindow::setupUI()
{
    // ── 좌측: 파일 목록 ──
    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(4, 4, 4, 4);

    QLabel *fileListLabel = new QLabel("📂 로그 파일 목록", leftPanel);
    fileListLabel->setStyleSheet("font-weight: bold; padding: 4px;");

    m_fileListWidget = new QListWidget(leftPanel);
    m_fileListWidget->setAlternatingRowColors(true);
    m_fileListWidget->setStyleSheet(
        "QListWidget::item:selected { background-color: #cce4ff; color: #000000; }"
        "QListWidget::item:hover    { background-color: #e8f4ff; color: #000000; }"
        );
    connect(m_fileListWidget, &QListWidget::itemClicked,
            this, &MainWindow::onFileSelected);

    leftLayout->addWidget(fileListLabel);
    leftLayout->addWidget(m_fileListWidget);

    // ── 우측 패널 ──
    QWidget *rightPanel = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(4, 4, 4, 4);
    rightLayout->setSpacing(4);

    QLabel *tableLabel = new QLabel("📋 파싱 결과", rightPanel);
    tableLabel->setStyleSheet("font-weight: bold; padding: 4px;");

    // ── 필터 바 ★ ──
    // ── 필터 바 (2줄) ★ ──
    QWidget *filterBarWrap = new QWidget(rightPanel);
    QVBoxLayout *filterWrapLayout = new QVBoxLayout(filterBarWrap);
    filterWrapLayout->setContentsMargins(0, 0, 0, 0);
    filterWrapLayout->setSpacing(3);

    const QString frameStyle =
        "QFrame { background-color: #f5f8fc; border: 1px solid #d0dce8;"
        "         border-radius: 6px; padding: 2px; }";
    const QString labelStyle = "font-weight: bold; color: #555;";

    auto makeSep = [&](QWidget *parent) -> QFrame* {
        QFrame *sep = new QFrame(parent);
        sep->setFrameShape(QFrame::VLine);
        sep->setStyleSheet("color: #ccc;");
        return sep;
    };

    // ── 1줄: 레벨 + 정렬 ──
    QFrame *row1 = new QFrame(filterBarWrap);
    row1->setFrameShape(QFrame::StyledPanel);
    row1->setStyleSheet(frameStyle);
    QHBoxLayout *row1Layout = new QHBoxLayout(row1);
    row1Layout->setContentsMargins(8, 4, 8, 4);
    row1Layout->setSpacing(8);

    QLabel *levelLabel = new QLabel("레벨:", row1);
    levelLabel->setStyleSheet(labelStyle);

    m_chkError = new QCheckBox("ERROR", row1);
    m_chkWarn  = new QCheckBox("WARN",  row1);
    m_chkInfo  = new QCheckBox("INFO",  row1);
    m_chkNoise = new QCheckBox("노이즈", row1);

    m_chkError->setChecked(true);
    m_chkWarn->setChecked(true);
    m_chkInfo->setChecked(true);
    m_chkNoise->setChecked(true);

    m_chkError->setStyleSheet("QCheckBox { color: #c0392b; font-weight: bold; }");
    m_chkWarn->setStyleSheet ("QCheckBox { color: #e67e22; font-weight: bold; }");
    m_chkInfo->setStyleSheet ("QCheckBox { color: #2980b9; font-weight: bold; }");
    m_chkNoise->setStyleSheet("QCheckBox { color: #888888; }");

    QLabel *sortLabel = new QLabel("정렬:", row1);
    sortLabel->setStyleSheet(labelStyle);

    m_sortCombo = new QComboBox(row1);
    m_sortCombo->addItem("원본 순서", "original");  // ★ 기본값 (맨 위)
    m_sortCombo->addItem("시간 오름차순 ↑", "asc");
    m_sortCombo->addItem("시간 내림차순 ↓", "desc");
    m_sortCombo->setStyleSheet("QComboBox { min-width: 130px; }");

    row1Layout->addWidget(levelLabel);
    row1Layout->addWidget(m_chkError);
    row1Layout->addWidget(m_chkWarn);
    row1Layout->addWidget(m_chkInfo);
    row1Layout->addWidget(m_chkNoise);
    row1Layout->addWidget(makeSep(row1));
    row1Layout->addWidget(sortLabel);
    row1Layout->addWidget(m_sortCombo);
    row1Layout->addWidget(makeSep(row1));   // ★ 검색 구분선

    // ★ 검색창
    QLabel *searchLabel = new QLabel("🔍", row1);
    m_searchEdit = new QLineEdit(row1);
    m_searchEdit->setPlaceholderText("검색어 입력...");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setStyleSheet("QLineEdit { min-width: 160px; padding: 2px 4px; border-radius: 4px; border: 1px solid #b0c8e0; }");

    m_searchScopeCombo = new QComboBox(row1);
    m_searchScopeCombo->addItem("전체",   "all");
    m_searchScopeCombo->addItem("모듈",   "module");
    m_searchScopeCombo->addItem("메시지", "message");
    m_searchScopeCombo->setStyleSheet("QComboBox { min-width: 70px; }");

    row1Layout->addWidget(searchLabel);
    row1Layout->addWidget(m_searchEdit);
    row1Layout->addWidget(m_searchScopeCombo);
    row1Layout->addStretch();

    // ── 2줄: 날짜 범위 + 시간 범위 + 초기화 ──
    QFrame *row2 = new QFrame(filterBarWrap);
    row2->setFrameShape(QFrame::StyledPanel);
    row2->setStyleSheet(frameStyle);
    QHBoxLayout *row2Layout = new QHBoxLayout(row2);
    row2Layout->setContentsMargins(8, 4, 8, 4);
    row2Layout->setSpacing(8);

    // 날짜 범위
    QLabel *dateLabel = new QLabel("날짜:", row2);
    dateLabel->setStyleSheet(labelStyle);

    m_dateFrom = new QDateEdit(row2);
    m_dateTo   = new QDateEdit(row2);
    m_dateFrom->setDisplayFormat("yyyy-MM-dd");
    m_dateTo->setDisplayFormat("yyyy-MM-dd");
    m_dateFrom->setCalendarPopup(true);
    m_dateTo->setCalendarPopup(true);
    m_dateFrom->setDate(QDate(2000, 1, 1));
    m_dateTo->setDate(QDate(2099, 12, 31));
    m_dateFrom->setStyleSheet("QDateEdit { min-width: 110px; }");
    m_dateTo->setStyleSheet  ("QDateEdit { min-width: 110px; }");

    QLabel *dateTild = new QLabel("~", row2);
    dateTild->setAlignment(Qt::AlignCenter);

    // 시간 범위
    QLabel *timeLabel = new QLabel("시간:", row2);
    timeLabel->setStyleSheet(labelStyle);

    m_timeFrom = new QTimeEdit(row2);
    m_timeTo   = new QTimeEdit(row2);
    m_timeFrom->setDisplayFormat("HH:mm:ss");
    m_timeTo->setDisplayFormat("HH:mm:ss");
    m_timeFrom->setTime(QTime(0, 0, 0));
    m_timeTo->setTime(QTime(23, 59, 59));
    m_timeFrom->setStyleSheet("QTimeEdit { min-width: 90px; }");
    m_timeTo->setStyleSheet  ("QTimeEdit { min-width: 90px; }");

    QLabel *timeTild = new QLabel("~", row2);
    timeTild->setAlignment(Qt::AlignCenter);

    // 초기화 버튼
    m_btnResetTime = new QPushButton("↺ 전체 초기화", row2);
    m_btnResetTime->setToolTip("레벨·검색·날짜·시간 전체 초기화");
    m_btnResetTime->setStyleSheet(
        "QPushButton { padding: 3px 8px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #d0e8ff; }"
        );

    row2Layout->addWidget(dateLabel);
    row2Layout->addWidget(m_dateFrom);
    row2Layout->addWidget(dateTild);
    row2Layout->addWidget(m_dateTo);
    row2Layout->addWidget(makeSep(row2));
    row2Layout->addWidget(timeLabel);
    row2Layout->addWidget(m_timeFrom);
    row2Layout->addWidget(timeTild);
    row2Layout->addWidget(m_timeTo);
    row2Layout->addWidget(m_btnResetTime);
    row2Layout->addStretch();

    filterWrapLayout->addWidget(row1);
    filterWrapLayout->addWidget(row2);

    // ── 시그널 연결 ──
    connect(m_chkError, &QCheckBox::checkStateChanged, this, &MainWindow::applyFilters);
    connect(m_chkWarn,  &QCheckBox::checkStateChanged, this, &MainWindow::applyFilters);
    connect(m_chkInfo,  &QCheckBox::checkStateChanged, this, &MainWindow::applyFilters);
    connect(m_chkNoise, &QCheckBox::checkStateChanged, this, &MainWindow::applyFilters);
    connect(m_dateFrom, &QDateEdit::dateChanged,       this, &MainWindow::applyFilters);
    connect(m_dateTo,   &QDateEdit::dateChanged,       this, &MainWindow::applyFilters);
    connect(m_timeFrom, &QTimeEdit::timeChanged,       this, &MainWindow::applyFilters);
    connect(m_timeTo,   &QTimeEdit::timeChanged,       this, &MainWindow::applyFilters);
    connect(m_sortCombo, &QComboBox::currentIndexChanged, this, &MainWindow::applyFilters);
    // ★ 검색 시그널
    connect(m_searchEdit,       &QLineEdit::textChanged,           this, &MainWindow::applyFilters);
    connect(m_searchScopeCombo, &QComboBox::currentIndexChanged,   this, &MainWindow::applyFilters);
    connect(m_btnResetTime, &QPushButton::clicked, this, [this]() {
        // 레벨 체크박스
        m_chkError->blockSignals(true);
        m_chkWarn->blockSignals(true);
        m_chkInfo->blockSignals(true);
        m_chkNoise->blockSignals(true);
        m_chkError->setChecked(true);
        m_chkWarn->setChecked(true);
        m_chkInfo->setChecked(true);
        m_chkNoise->setChecked(true);
        m_chkError->blockSignals(false);
        m_chkWarn->blockSignals(false);
        m_chkInfo->blockSignals(false);
        m_chkNoise->blockSignals(false);

        // 정렬
        m_sortCombo->blockSignals(true);
        m_sortCombo->setCurrentIndex(0);  // 원본 순서
        m_sortCombo->blockSignals(false);

        // 검색
        m_searchEdit->blockSignals(true);
        m_searchEdit->clear();
        m_searchEdit->blockSignals(false);
        m_searchScopeCombo->blockSignals(true);
        m_searchScopeCombo->setCurrentIndex(0);  // 전체
        m_searchScopeCombo->blockSignals(false);

        // 날짜/시간
        m_dateFrom->blockSignals(true);
        m_dateTo->blockSignals(true);
        m_timeFrom->blockSignals(true);
        m_timeTo->blockSignals(true);
        m_dateFrom->setDate(QDate(2000, 1, 1));
        m_dateTo->setDate(QDate(2099, 12, 31));
        m_timeFrom->setTime(QTime(0, 0, 0));
        m_timeTo->setTime(QTime(23, 59, 59));
        m_dateFrom->blockSignals(false);
        m_dateTo->blockSignals(false);
        m_timeFrom->blockSignals(false);
        m_timeTo->blockSignals(false);

        applyFilters();
    });

    // ── 테이블 위젯 ──
    m_logTableWidget = new QTableWidget(0, 5, rightPanel);
    m_logTableWidget->setHorizontalHeaderLabels({"날짜", "시간", "레벨", "모듈", "메시지"});
    m_logTableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_logTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_logTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logTableWidget->setAlternatingRowColors(true);
    m_logTableWidget->setStyleSheet(
        "QTableWidget::item:selected { background-color: #cce4ff; color: #000000; }"
        "QTableWidget::item:hover    { background-color: #e8f4ff; color: #000000; }"
        );

    rightLayout->addWidget(tableLabel);
    rightLayout->addWidget(filterBarWrap);      // ★ 필터 바
    rightLayout->addWidget(m_logTableWidget);

    // ── 스플리터 ──
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setHandleWidth(4);

    setCentralWidget(splitter);
}

// ── setupFilterBar 별도 함수로 빼지 않음 (setupUI 내에 통합) ──
void MainWindow::setupFilterBar() {} // 빈 구현 (헤더 선언 유지용)

void MainWindow::setupToolBar()
{
    QToolBar *toolBar = addToolBar("메인 툴바");
    toolBar->setMovable(false);

    m_addFileBtn    = new QPushButton("📄 파일 추가", this);
    m_removeFileBtn = new QPushButton("🗑 파일 제거", this);
    m_parseBtn      = new QPushButton("▶ 파싱 시작", this);
    m_csvBtn        = new QPushButton("📥 CSV 내보내기", this);
    m_reportBtn     = new QPushButton("📊 리포트 생성", this);

    const QString btnStyle =
        "QPushButton { padding: 5px 12px; margin: 2px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #d0e8ff; }"
        "QPushButton:disabled { color: #aaaaaa; }";

    m_addFileBtn->setStyleSheet(btnStyle);
    m_removeFileBtn->setStyleSheet(btnStyle);
    m_parseBtn->setStyleSheet(btnStyle);
    m_csvBtn->setStyleSheet(btnStyle);
    m_reportBtn->setStyleSheet(btnStyle);

    m_parseBtn->setVisible(false);

    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    toolBar->addWidget(m_addFileBtn);
    toolBar->addWidget(m_removeFileBtn);
    toolBar->addWidget(m_parseBtn);
    toolBar->addWidget(spacer);
    toolBar->addWidget(m_csvBtn);
    toolBar->addWidget(m_reportBtn);

    connect(m_addFileBtn,    &QPushButton::clicked, this, &MainWindow::onAddFile);
    connect(m_removeFileBtn, &QPushButton::clicked, this, &MainWindow::onRemoveFile);
    connect(m_parseBtn,      &QPushButton::clicked, this, &MainWindow::onParseFile);
    connect(m_csvBtn,        &QPushButton::clicked, this, &MainWindow::onExportCsv);
    connect(m_reportBtn,     &QPushButton::clicked, this, &MainWindow::onGenerateReport);
}

// ── 파일 추가 ────────────────────────────────────────────
void MainWindow::onAddFile()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, "로그 파일 선택", "",
        "로그/CSV 파일 (*.log *.txt *.csv);;모든 파일 (*)");

    for (const QString &path : files) {
        // 전체 경로로 중복 체크 (툴팁 기준)
        bool alreadyExists = false;
        for (int i = 0; i < m_fileListWidget->count(); ++i) {
            if (m_fileListWidget->item(i)->toolTip() == path) {
                alreadyExists = true;
                break;
            }
        }
        if (alreadyExists) continue;

        QListWidgetItem *item = new QListWidgetItem(QFileInfo(path).fileName());
        item->setToolTip(path);   // 전체 경로는 툴팁으로
        m_fileListWidget->addItem(item);
    }
}

// ── 파일 제거 ────────────────────────────────────────────
void MainWindow::onRemoveFile()
{
    QListWidgetItem *selected = m_fileListWidget->currentItem();
    if (!selected) {
        QMessageBox::information(this, "알림", "제거할 파일을 선택하세요.");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "파일 제거",
        QString("'%1'\n(%2)\n목록에서 제거하시겠습니까?")
            .arg(selected->text())       // 파일명
            .arg(selected->toolTip()),    // 전체 경로
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    delete m_fileListWidget->takeItem(m_fileListWidget->row(selected));
    m_logTableWidget->setRowCount(0);
    m_allEntries.clear();   // ★
    m_parseBtn->setVisible(false);
    m_currentFile.clear();
    statusBar()->showMessage("파일이 제거되었습니다.");
}

// ── 파일 선택 시 ─────────────────────────────────────────
void MainWindow::onFileSelected(QListWidgetItem *item)
{
    m_currentFile = item->toolTip();
    QFileInfo fi(m_currentFile);

    if (fi.size() <= AUTO_PARSE_LIMIT) {
        m_parseBtn->setVisible(false);
        statusBar()->showMessage(QString("파싱 중... (%1)").arg(fi.fileName()));
        parseAndDisplay(m_currentFile);
    } else {
        m_parseBtn->setVisible(true);
        m_logTableWidget->setRowCount(0);
        m_allEntries.clear();
        statusBar()->showMessage(
            QString("대용량 파일 (%1 MB) — [▶ 파싱 시작] 버튼을 누르세요.")
                .arg(fi.size() / 1024.0 / 1024.0, 0, 'f', 1));
    }
}

// ── 수동 파싱 버튼 ───────────────────────────────────────
void MainWindow::onParseFile()
{
    if (m_currentFile.isEmpty()) return;
    statusBar()->showMessage("파싱 중...");
    parseAndDisplay(m_currentFile);
    m_parseBtn->setVisible(false);
}

// ── 실제 파싱 + 테이블 표시 ──────────────────────────────
void MainWindow::parseAndDisplay(const QString &filePath)
{
    m_allEntries = m_parser.parse(filePath);  // ★ 전체 보관

    // 파싱 결과에서 시간 범위 자동 감지 → DateTimeEdit 초기값 설정
    // (노이즈 제외하고 유효한 날짜만)
    QDateTime minDt, maxDt;
    for (const LogEntry &e : m_allEntries) {
        if (!e.parsed) continue;
        // "yyyy-MM-dd" + "HH:mm:ss.zzz" 조합
        QString dtStr = e.date + " " + e.timestamp;
        QDateTime dt = QDateTime::fromString(dtStr, "yyyy-MM-dd HH:mm:ss.zzz");
        if (!dt.isValid())
            dt = QDateTime::fromString(dtStr, "yyyy-MM-dd HH:mm:ss");
        if (!dt.isValid()) continue;

        if (!minDt.isValid() || dt < minDt) minDt = dt;
        if (!maxDt.isValid() || dt > maxDt) maxDt = dt;
    }

    // 유효한 범위가 감지되면 DateTimeEdit에 반영 (시그널 블록)
    if (minDt.isValid() && maxDt.isValid()) {
        m_dateFrom->blockSignals(true);
        m_dateTo->blockSignals(true);
        m_timeFrom->blockSignals(true);
        m_timeTo->blockSignals(true);
        m_dateFrom->setDate(minDt.date());
        m_dateTo->setDate(maxDt.date());
        m_timeFrom->setTime(minDt.time());
        m_timeTo->setTime(maxDt.time());
        m_dateFrom->blockSignals(false);
        m_dateTo->blockSignals(false);
        m_timeFrom->blockSignals(false);
        m_timeTo->blockSignals(false);
    }

    applyFilters();  // ★ 필터 적용해서 테이블 갱신

    QFileInfo fi(filePath);
    statusBar()->showMessage(
        QString("%1 — %2줄 파싱 완료 (노이즈: %3줄)")
            .arg(fi.fileName())
            .arg(m_allEntries.size())
            .arg(m_parser.noiseCount()));
    setWindowTitle("TellMeLog — " + fi.fileName());
}

// ── 필터 + 정렬 적용 ★ ──────────────────────────────────
void MainWindow::applyFilters()
{
    if (m_allEntries.isEmpty()) return;

    bool showError = m_chkError->isChecked();
    bool showWarn  = m_chkWarn->isChecked();
    bool showInfo  = m_chkInfo->isChecked();
    bool showNoise = m_chkNoise->isChecked();
    QString sortMode = m_sortCombo->currentData().toString();

    // ★ 검색어
    QString keyword     = m_searchEdit->text().trimmed();
    QString searchScope = m_searchScopeCombo->currentData().toString();

    QDateTime dtFrom(m_dateFrom->date(), m_timeFrom->time());
    QDateTime dtTo  (m_dateTo->date(),   m_timeTo->time());

    // 필터링
    QVector<LogEntry> filtered;
    filtered.reserve(m_allEntries.size());

    for (const LogEntry &e : m_allEntries) {
        // 레벨 필터
        if (!e.parsed) {
            if (!showNoise) continue;
        } else if (e.level == "ERROR") {
            if (!showError) continue;
        } else if (e.level == "WARN") {
            if (!showWarn) continue;
        } else {
            // INFO, DEBUG 등 나머지
            if (!showInfo) continue;
        }

        // 시간 범위 필터 (파싱된 줄만 적용, 노이즈는 통과)
        if (e.parsed) {
            QString dtStr = e.date + " " + e.timestamp;
            QDateTime dt = QDateTime::fromString(dtStr, "yyyy-MM-dd HH:mm:ss.zzz");
            if (!dt.isValid())
                dt = QDateTime::fromString(dtStr, "yyyy-MM-dd HH:mm:ss");
            if (dt.isValid()) {
                if (dt < dtFrom || dt > dtTo) continue;
            }
        }

        // ★ 검색 필터
        if (!keyword.isEmpty()) {
            bool hit = false;
            if (searchScope == "module") {
                hit = e.module.contains(keyword, Qt::CaseInsensitive);
            } else if (searchScope == "message") {
                hit = e.message.contains(keyword, Qt::CaseInsensitive);
            } else { // "all"
                hit = e.module.contains(keyword, Qt::CaseInsensitive)
                      || e.message.contains(keyword, Qt::CaseInsensitive)
                      || e.level.contains(keyword, Qt::CaseInsensitive)
                      || e.date.contains(keyword, Qt::CaseInsensitive)
                      || e.timestamp.contains(keyword, Qt::CaseInsensitive);
            }
            if (!hit) continue;
        }

        filtered.append(e);
    }

    // 정렬 (시간 기준)
    auto toDateTime = [](const LogEntry &e) -> QDateTime {
        // DD/MM/YYYY 형식은 yyyy-MM-dd 로 정규화 후 파싱
        QString dateStr = e.date;
        static const QRegularExpression reDMY(R"(^(\d{2})/(\d{2})/(\d{4})$)");
        QRegularExpressionMatch m = reDMY.match(dateStr);
        if (m.hasMatch())
            dateStr = m.captured(3) + "-" + m.captured(2) + "-" + m.captured(1);

        QString dtStr = dateStr + " " + e.timestamp;

        QDateTime dt = QDateTime::fromString(dtStr, "yyyy-MM-dd HH:mm:ss.zzz");
        if (!dt.isValid())
            dt = QDateTime::fromString(dtStr, "yyyy-MM-dd HH:mm:ss");
        if (!dt.isValid())
            dt = QDateTime::fromString(dtStr, "yyyy/MM/dd HH:mm:ss.zzz");
        if (!dt.isValid())
            dt = QDateTime::fromString(dtStr, "yyyy/MM/dd HH:mm:ss");
        return dt;
    };

    if (sortMode != "original") {
        std::sort(filtered.begin(), filtered.end(),
                  [&](const LogEntry &a, const LogEntry &b) {
                      QDateTime da = toDateTime(a);
                      QDateTime db = toDateTime(b);
                      if (!da.isValid() && !db.isValid()) return false;
                      if (!da.isValid()) return false;
                      if (!db.isValid()) return true;
                      return (sortMode == "desc") ? da > db : da < db;
                  });
    }
    // "original"이면 filtered는 m_allEntries 순서 그대로 (필터링만 적용)

    populateTable(filtered);

    // 상태바에 필터 결과 반영
    QString statusMsg = QString("표시: %1줄 / 전체: %2줄")
                            .arg(filtered.size())
                            .arg(m_allEntries.size());
    if (!keyword.isEmpty())
        statusMsg += QString("  |  🔍 \"%1\" (%2 범위)").arg(keyword).arg(m_searchScopeCombo->currentText());
    statusBar()->showMessage(statusMsg);
}

// ── 테이블 채우기 ────────────────────────────────────────
void MainWindow::populateTable(const QVector<LogEntry> &entries)
{
    m_logTableWidget->setRowCount(0);

    for (const LogEntry &entry : entries) {
        int row = m_logTableWidget->rowCount();
        m_logTableWidget->insertRow(row);

        m_logTableWidget->setItem(row, 0, new QTableWidgetItem(entry.date));
        m_logTableWidget->setItem(row, 1, new QTableWidgetItem(entry.timestamp));
        m_logTableWidget->setItem(row, 2, new QTableWidgetItem(entry.level));
        m_logTableWidget->setItem(row, 3, new QTableWidgetItem(entry.module));
        m_logTableWidget->setItem(row, 4, new QTableWidgetItem(entry.message));

        QColor rowColor;
        if (!entry.parsed) {
            rowColor = QColor("#f0f0f0");
        } else if (entry.level == "ERROR") {
            rowColor = QColor("#ffe0e0");
        } else if (entry.level == "WARN") {
            rowColor = QColor("#fff4cc");
        }

        if (rowColor.isValid()) {
            for (int col = 0; col < 5; ++col)
                m_logTableWidget->item(row, col)->setBackground(rowColor);
        }
    }
}

// ── CSV 내보내기 ─────────────────────────────────────────
void MainWindow::onExportCsv()
{
    if (m_logTableWidget->rowCount() == 0) {
        QMessageBox::information(this, "알림", "내보낼 데이터가 없습니다.");
        return;
    }

    QFileInfo fi(m_currentFile);
    QString defaultName = m_currentFile.isEmpty()
                              ? "export.csv"
                              : fi.completeBaseName() + ".csv";
    QString defaultDir = m_currentFile.isEmpty()
                             ? QDir::homePath()
                             : fi.absolutePath();

    QString savePath = QFileDialog::getSaveFileName(
        this, "CSV 저장", defaultDir + "/" + defaultName, "CSV 파일 (*.csv)");

    if (savePath.isEmpty()) return;

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "오류", "파일을 저장할 수 없습니다:\n" + savePath);
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "\xEF\xBB\xBF";
    out << "날짜,시간,레벨,모듈,메시지\n";

    for (int row = 0; row < m_logTableWidget->rowCount(); ++row) {
        QStringList fields;
        for (int col = 0; col < 5; ++col) {
            QTableWidgetItem *cell = m_logTableWidget->item(row, col);
            QString text = cell ? cell->text() : QString();
            if (text.contains(',') || text.contains('"') || text.contains('\n')) {
                text.replace("\"", "\"\"");
                text = "\"" + text + "\"";
            }
            fields.append(text);
        }
        out << fields.join(',') << '\n';
    }

    file.close();
    statusBar()->showMessage(
        QString("CSV 저장 완료: %1 (%2행)")
            .arg(QFileInfo(savePath).fileName())
            .arg(m_logTableWidget->rowCount()));
}

void MainWindow::onGenerateReport()
{
    if (m_allEntries.isEmpty()) {
        QMessageBox::information(this, "알림", "파싱된 로그가 없습니다.");
        return;
    }
    m_reportGen.generate(this, m_allEntries, m_currentFile);
}