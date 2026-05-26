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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("TellMeLog - 로그야 뭐라고");
    resize(1100, 650);
    setupUI();
    setupToolBar();

    // 상태바 초기 메시지
    statusBar()->showMessage("파일을 추가하세요.");
}

MainWindow::~MainWindow() {}

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

    // ── 우측: 로그 테이블 ──
    QWidget *rightPanel = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(4, 4, 4, 4);

    QLabel *tableLabel = new QLabel("📋 파싱 결과", rightPanel);
    tableLabel->setStyleSheet("font-weight: bold; padding: 4px;");

    m_logTableWidget = new QTableWidget(0, 5, rightPanel);
    m_logTableWidget->setHorizontalHeaderLabels({"날짜", "시간", "레벨", "모듈", "메시지"});
    m_logTableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch); // 메시지 컬럼 인덱스 4로 변경
    m_logTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_logTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logTableWidget->setAlternatingRowColors(true);
    m_logTableWidget->setStyleSheet(
        "QTableWidget::item:selected { background-color: #cce4ff; color: #000000; }"
        "QTableWidget::item:hover    { background-color: #e8f4ff; color: #000000; }"
        );

    rightLayout->addWidget(tableLabel);
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

    // 파싱 버튼은 초기에 숨김 (대용량 파일 선택 시에만 표시)
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
        "로그/CSV 파일 (*.log *.txt *.csv);;모든 파일 (*)");  // ★ *.csv 추가

    for (const QString &path : files) {
        if (m_fileListWidget->findItems(path, Qt::MatchExactly).isEmpty())
            m_fileListWidget->addItem(path);
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

    // 확인 다이얼로그
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "파일 제거",
        QString("'%1'\n목록에서 제거하시겠습니까?").arg(selected->text()),
        QMessageBox::Yes | QMessageBox::No
        );

    if (reply != QMessageBox::Yes)
        return;

    delete m_fileListWidget->takeItem(m_fileListWidget->row(selected));
    m_logTableWidget->setRowCount(0);
    m_parseBtn->setVisible(false);
    m_currentFile.clear();
    statusBar()->showMessage("파일이 제거되었습니다.");
}

// ── 파일 선택 시 ─────────────────────────────────────────
void MainWindow::onFileSelected(QListWidgetItem *item)
{
    m_currentFile = item->text();
    QFileInfo fi(m_currentFile);

    if (fi.size() <= AUTO_PARSE_LIMIT) {
        // 소용량 → 즉시 자동 파싱
        m_parseBtn->setVisible(false);
        statusBar()->showMessage(QString("파싱 중... (%1)").arg(fi.fileName()));
        parseAndDisplay(m_currentFile);
    } else {
        // 대용량 → 버튼 표시하고 대기
        m_parseBtn->setVisible(true);
        m_logTableWidget->setRowCount(0);
        statusBar()->showMessage(
            QString("대용량 파일 (%1 MB) — [▶ 파싱 시작] 버튼을 누르세요.")
                .arg(fi.size() / 1024.0 / 1024.0, 0, 'f', 1)
            );
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
    QVector<LogEntry> entries = m_parser.parse(filePath);
    populateTable(entries);

    QFileInfo fi(filePath);
    statusBar()->showMessage(
        QString("%1 — %2줄 파싱 완료 (노이즈: %3줄)")
            .arg(fi.fileName())
            .arg(entries.size())
            .arg(m_parser.noiseCount())
        );
    setWindowTitle("TellMeLog — " + fi.fileName());
}

// ── 테이블 채우기 ────────────────────────────────────────
void MainWindow::populateTable(const QVector<LogEntry> &entries)
{
    m_logTableWidget->setRowCount(0); // 기존 내용 초기화

    for (const LogEntry &entry : entries) {
        int row = m_logTableWidget->rowCount();
        m_logTableWidget->insertRow(row);

        m_logTableWidget->setItem(row, 0, new QTableWidgetItem(entry.date));
        m_logTableWidget->setItem(row, 1, new QTableWidgetItem(entry.timestamp));
        m_logTableWidget->setItem(row, 2, new QTableWidgetItem(entry.level));
        m_logTableWidget->setItem(row, 3, new QTableWidgetItem(entry.module));
        m_logTableWidget->setItem(row, 4, new QTableWidgetItem(entry.message));

        // 레벨별 행 색상
        QColor rowColor;
        if (!entry.parsed) {
            rowColor = QColor("#f0f0f0"); // 노이즈: 회색
        } else if (entry.level == "ERROR") {
            rowColor = QColor("#ffe0e0"); // 빨강 계열
        } else if (entry.level == "WARN") {
            rowColor = QColor("#fff4cc"); // 노랑 계열
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

    // 기본 파일명: 원본 로그 파일명 + .csv
    QFileInfo fi(m_currentFile);
    QString defaultName = m_currentFile.isEmpty()
                              ? "export.csv"
                              : fi.completeBaseName() + ".csv";

    QString defaultDir = m_currentFile.isEmpty()
                             ? QDir::homePath()
                             : QFileInfo(m_currentFile).absolutePath();

    QString savePath = QFileDialog::getSaveFileName(
        this, "CSV 저장", defaultDir + "/" + defaultName, "CSV 파일 (*.csv)");

    if (savePath.isEmpty())
        return;

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "오류",
                             "파일을 저장할 수 없습니다:\n" + savePath);
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // BOM 추가 — 엑셀에서 한글 깨짐 방지
    out << "\xEF\xBB\xBF";

    // 헤더
    out << "날짜,시간,레벨,모듈,메시지\n";

    // 데이터 행
    for (int row = 0; row < m_logTableWidget->rowCount(); ++row) {
        QStringList fields;
        for (int col = 0; col < 5; ++col) {
            QTableWidgetItem *cell = m_logTableWidget->item(row, col);
            QString text = cell ? cell->text() : QString();

            // RFC 4180: 쉼표/따옴표/줄바꿈 포함 시 따옴표로 감싸기
            if (text.contains(',') || text.contains('"') || text.contains('\n')) {
                text.replace("\"", "\"\"");   // 따옴표 이스케이프
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
            .arg(m_logTableWidget->rowCount())
        );
}
void MainWindow::onGenerateReport()
{
    QMessageBox::information(this, "리포트", "리포트 생성 기능은 추후 구현됩니다.");
}