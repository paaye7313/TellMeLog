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
#include <QLineEdit>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QApplication>
#include <QMouseEvent>
#include <QEvent>
#include <QFileSystemWatcher>  // ★ 추가
#include <QTimer>              // ★ 추가
#include <QtConcurrent>
#include <QFutureWatcher>

// ── 파일 목록 커스텀 델리게이트 ─────────────────────────────
// 역할:
//   1) 아이템 배경을 파일 고유 색으로 채움 (선택/호버도 해당 색 기준)
//   2) 왼쪽에 체크박스를 직접 그림 (Qt 체크박스 스타일 사용)
//   3) 체크박스 영역 / 텍스트 영역을 분리해서 클릭 구분에 사용
//   4) ★ 감시 중인 파일은 텍스트 앞에 🟢 표시
class FileListDelegate : public QStyledItemDelegate
{
public:
    static constexpr int CHECKBOX_WIDTH = 26;

    explicit FileListDelegate(const QMap<QString, QColor> *colorMap,
                              const QSet<QString>         *watchedFiles,
                              QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
        , m_colorMap(colorMap)
        , m_watchedFiles(watchedFiles)
    {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QString filePath = index.data(Qt::ToolTipRole).toString();
        bool checked = (index.data(Qt::CheckStateRole).toInt() == Qt::Checked);
        bool watching = m_watchedFiles->contains(filePath); // ★

        QColor baseColor = checked
                               ? m_colorMap->value(filePath, QColor("#FFFFFF"))
                               : QColor("#FFFFFF");

        QColor bgColor;
        if (option.state & QStyle::State_Selected)
            bgColor = checked ? baseColor.darker(115) : QColor("#EEEEEE");
        else if (option.state & QStyle::State_MouseOver)
            bgColor = checked ? baseColor.darker(105) : QColor("#F5F5F5");
        else
            bgColor = baseColor;

        painter->save();

        // 1) 배경
        painter->fillRect(option.rect, bgColor);

        // 2) 체크박스
        QStyleOptionButton cbOpt;
        cbOpt.rect = checkBoxRect(option.rect);
        cbOpt.state = QStyle::State_Enabled;
        cbOpt.state |= checked ? QStyle::State_On : QStyle::State_Off;
        QApplication::style()->drawPrimitive(QStyle::PE_IndicatorCheckBox, &cbOpt, painter);

        // 3) 텍스트 (감시 중이면 🟢 prefix 붙임)
        QRect textRect = option.rect.adjusted(CHECKBOX_WIDTH + 6, 0, -4, 0);
        QString displayText = index.data(Qt::DisplayRole).toString();
        if (watching)
            displayText = "🟢 " + displayText;  // ★ 감시 중 표시

        painter->setPen(QColor("#000000"));
        painter->setFont(option.font);
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                          option.fontMetrics.elidedText(displayText, Qt::ElideRight, textRect.width()));

        painter->restore();
    }

    static QRect checkBoxRect(const QRect &itemRect)
    {
        int cbSize = 16;
        int x = itemRect.left() + 5;
        int y = itemRect.top() + (itemRect.height() - cbSize) / 2;
        return QRect(x, y, cbSize, cbSize);
    }

private:
    const QMap<QString, QColor> *m_colorMap;
    const QSet<QString>         *m_watchedFiles; // ★
};

// ─────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("TellMeLog - 로그야 뭐라고");
    resize(1200, 700);

    // ★ QFileSystemWatcher 초기화
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this,      &MainWindow::onFileChanged);

    // ★ 하이라이트 페이드 타이머 초기화
    m_highlightTimer = new QTimer(this);
    m_highlightTimer->setInterval(FADE_INTERVAL);
    connect(m_highlightTimer, &QTimer::timeout, this, [this]() {
        if (m_pendingHighlightRanges.isEmpty()) {
            m_highlightTimer->stop();
            return;
        }

        ++m_highlightStep;
        // 페이드: 노란색 #fffacd → 흰색 (FADE_STEPS 단계)
        // step=0: 노랑 진하게, step=FADE_STEPS: 원래 행 색으로
        float ratio = static_cast<float>(m_highlightStep) / FADE_STEPS;
        ratio = qMin(ratio, 1.0f);

        for (const auto &range : m_pendingHighlightRanges) {
            for (int row = range.first; row <= range.second; ++row) {
                if (row >= m_logTableWidget->rowCount()) continue;

                // 이 행의 기본 색 결정 (ERROR/WARN/노이즈/파일색/흰색)
                // populateTable과 동일한 규칙
                QColor targetColor;
                QTableWidgetItem *lvItem = m_logTableWidget->item(row, 2);
                QString level = lvItem ? lvItem->text() : QString();

                // 노이즈는 배경으로 판단하기 어려우므로 흰색 기준
                if (level == "ERROR")
                    targetColor = QColor("#ffe0e0");
                else if (level == "WARN")
                    targetColor = QColor("#fff4cc");
                else
                    targetColor = QColor("#FFFFFF");

                // 노란색(255,255,180) → targetColor 선형 보간
                int r = static_cast<int>(255 + (targetColor.red()   - 255) * ratio);
                int g = static_cast<int>(255 + (targetColor.green() - 255) * ratio);
                int b = static_cast<int>(180 + (targetColor.blue()  - 180) * ratio);
                QColor fadeColor(r, g, b);

                for (int col = 0; col < 5; ++col) {
                    if (auto *cell = m_logTableWidget->item(row, col))
                        cell->setBackground(fadeColor);
                }
            }
        }

        if (m_highlightStep >= FADE_STEPS) {
            m_highlightTimer->stop();
            m_pendingHighlightRanges.clear();
            m_highlightStep = 0;
        }
    });

    setupUI();
    setupToolBar();
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setFixedWidth(200);
    m_progressBar->setVisible(false);
    statusBar()->addPermanentWidget(m_progressBar);
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
    m_fileListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fileListWidget->setMouseTracking(true);
    // ★ 커스텀 델리게이트: m_watchedFiles 포인터 전달
    m_fileListWidget->setItemDelegate(
        new FileListDelegate(&m_fileColors, &m_watchedFiles, m_fileListWidget));
    m_fileListWidget->viewport()->installEventFilter(this);

    leftLayout->addWidget(fileListLabel);
    leftLayout->addWidget(m_fileListWidget);

    // ── 우측 패널 ──
    QWidget *rightPanel = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(4, 4, 4, 4);
    rightLayout->setSpacing(4);

    QLabel *tableLabel = new QLabel("📋 파싱 결과", rightPanel);
    tableLabel->setStyleSheet("font-weight: bold; padding: 4px;");

    // ── 필터 바 (2줄) ──
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
    m_sortCombo->addItem("원본 순서", "original");
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
    row1Layout->addWidget(makeSep(row1));

    // 검색창
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
    connect(m_searchEdit,       &QLineEdit::textChanged,           this, &MainWindow::applyFilters);
    connect(m_searchScopeCombo, &QComboBox::currentIndexChanged,   this, &MainWindow::applyFilters);
    connect(m_btnResetTime, &QPushButton::clicked, this, [this]() {
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

        m_sortCombo->blockSignals(true);
        m_sortCombo->setCurrentIndex(0);
        m_sortCombo->blockSignals(false);

        m_searchEdit->blockSignals(true);
        m_searchEdit->clear();
        m_searchEdit->blockSignals(false);
        m_searchScopeCombo->blockSignals(true);
        m_searchScopeCombo->setCurrentIndex(0);
        m_searchScopeCombo->blockSignals(false);

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
    rightLayout->addWidget(filterBarWrap);
    rightLayout->addWidget(m_logTableWidget);

    // ── 스플리터 ──
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setHandleWidth(4);
    splitter->setSizes({240, 960});

    setCentralWidget(splitter);
}

void MainWindow::setupFilterBar() {}

// ── 파일 목록 클릭 처리 ────────────────────────────────
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_fileListWidget->viewport()
        && event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            QListWidgetItem *item = m_fileListWidget->itemAt(me->pos());
            if (!item) return false;

            QRect itemRect = m_fileListWidget->visualItemRect(item);
            QRect cbRect   = FileListDelegate::checkBoxRect(itemRect);

            if (cbRect.contains(me->pos())) {
                item->setCheckState(
                    item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);

                QStringList checkedPaths;
                for (int i = 0; i < m_fileListWidget->count(); ++i) {
                    auto *it = m_fileListWidget->item(i);
                    if (it->checkState() == Qt::Checked)
                        checkedPaths << it->toolTip();
                }

                if (checkedPaths.isEmpty()) {
                    m_logTableWidget->setRowCount(0);
                    m_allEntries.clear();
                    statusBar()->showMessage("파일을 체크하거나 선택하세요.");
                } else if (checkedPaths.size() == 1) {
                    m_currentFile = checkedPaths.first();
                    QFileInfo fi(m_currentFile);
                    if (fi.size() <= AUTO_PARSE_LIMIT) {
                        parseAndDisplay(m_currentFile);
                    } else {
                        m_parseBtnAction->setVisible(true);
                        m_toolBar->layout()->invalidate();
                        m_toolBar->repaint();
                        m_logTableWidget->setRowCount(0);
                        m_allEntries.clear();

                        qDebug() << "parseBtn visible:" << m_parseBtn->isVisible();
                        qDebug() << "parseBtn geometry:" << m_parseBtn->geometry();

                        statusBar()->showMessage(
                            QString("대용량 파일 (%1 MB) — [▶ 파싱 시작] 버튼을 누르세요.")
                                .arg(fi.size() / 1024.0 / 1024.0, 0, 'f', 1));
                    }
                } else {
                    mergeAndDisplay(checkedPaths);
                }
                return true;

            } else {
                for (int i = 0; i < m_fileListWidget->count(); ++i)
                    m_fileListWidget->item(i)->setCheckState(Qt::Unchecked);
                item->setCheckState(Qt::Checked);
                onFileSelected(item);
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

// ── 툴바 구성 ★ 감시 버튼 추가 ────────────────────────────
void MainWindow::setupToolBar()
{
    m_toolBar = addToolBar("메인 툴바");
    m_toolBar->setMovable(false);

    m_addFileBtn    = new QPushButton("📄 파일 추가", this);
    m_removeFileBtn = new QPushButton("🗑 파일 제거", this);
    m_parseBtn      = new QPushButton("▶ 파싱 시작", this);
    m_csvBtn        = new QPushButton("📥 CSV 내보내기", this);
    m_reportBtn     = new QPushButton("📊 리포트 생성", this);
    m_watchBtn      = new QPushButton("👁 감시 시작", this); // ★

    const QString btnStyle =
        "QPushButton { padding: 5px 12px; margin: 2px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #d0e8ff; }"
        "QPushButton:disabled { color: #aaaaaa; }";

    // ★ 감시 버튼은 활성화 시 초록 배경으로 구분
    const QString watchBtnActiveStyle =
        "QPushButton { padding: 5px 12px; margin: 2px; border-radius: 4px;"
        "              background-color: #d4efdf; border: 1px solid #27ae60; color: #1a7a40; font-weight: bold; }"
        "QPushButton:hover { background-color: #abebc6; }";

    m_addFileBtn->setStyleSheet(btnStyle);
    m_removeFileBtn->setStyleSheet(btnStyle);
    m_parseBtn->setStyleSheet(btnStyle);
    m_csvBtn->setStyleSheet(btnStyle);
    m_reportBtn->setStyleSheet(btnStyle);
    m_watchBtn->setStyleSheet(btnStyle);  // 초기: 비활성 스타일

    // ★ 감시 버튼 스타일 프로퍼티 저장 (토글에 사용)
    m_watchBtn->setProperty("activeStyle",   watchBtnActiveStyle);
    m_watchBtn->setProperty("inactiveStyle", btnStyle);

    m_parseBtn->setStyleSheet(btnStyle);
    m_parseBtnAction = m_toolBar->addWidget(m_parseBtn);
    m_parseBtnAction->setVisible(false);

    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_toolBar->addWidget(m_addFileBtn);
    m_toolBar->addWidget(m_removeFileBtn);
    m_toolBar->addWidget(m_watchBtn);   // ★ 파싱 버튼 바로 뒤에 배치
    m_toolBar->addWidget(spacer);
    m_toolBar->addWidget(m_csvBtn);
    m_toolBar->addWidget(m_reportBtn);

    connect(m_addFileBtn,    &QPushButton::clicked, this, &MainWindow::onAddFile);
    connect(m_removeFileBtn, &QPushButton::clicked, this, &MainWindow::onRemoveFile);
    connect(m_parseBtn, &QPushButton::clicked, this, &MainWindow::onParseFile);
    connect(m_csvBtn,        &QPushButton::clicked, this, &MainWindow::onExportCsv);
    connect(m_reportBtn,     &QPushButton::clicked, this, &MainWindow::onGenerateReport);
    connect(m_watchBtn,      &QPushButton::clicked, this, &MainWindow::onToggleWatch); // ★
}

// ── 파일 추가 ─────────────────────────────────────────────
void MainWindow::onAddFile()
{
    static const QList<QColor> FILE_PALETTE = {
        QColor("#FFFFFF"),
        QColor("#EBF5FB"),
        QColor("#E8F8F5"),
        QColor("#F5EEF8"),
        QColor("#FEF9E7"),
        QColor("#FDFEFE"),
    };

    QStringList files = QFileDialog::getOpenFileNames(
        this, "로그 파일 선택", "",
        "로그/CSV 파일 (*.log *.txt *.csv);;모든 파일 (*)");

    for (const QString &path : files) {
        bool alreadyExists = false;
        for (int i = 0; i < m_fileListWidget->count(); ++i) {
            if (m_fileListWidget->item(i)->toolTip() == path) {
                alreadyExists = true;
                break;
            }
        }
        if (alreadyExists) continue;

        int colorIdx = m_fileColors.size() % FILE_PALETTE.size();
        QColor assignedColor = FILE_PALETTE[colorIdx];
        m_fileColors[path] = assignedColor;

        QListWidgetItem *item = new QListWidgetItem(QFileInfo(path).fileName());
        item->setToolTip(path);
        item->setCheckState(Qt::Unchecked);
        m_fileListWidget->addItem(item);
    }
}

// ── 파일 제거 ─────────────────────────────────────────────
void MainWindow::onRemoveFile()
{
    QList<QListWidgetItem*> targets;
    for (int i = 0; i < m_fileListWidget->count(); ++i) {
        if (m_fileListWidget->item(i)->checkState() == Qt::Checked)
            targets << m_fileListWidget->item(i);
    }
    if (targets.isEmpty()) {
        QListWidgetItem *cur = m_fileListWidget->currentItem();
        if (cur) targets << cur;
    }
    if (targets.isEmpty()) {
        QMessageBox::information(this, "알림", "제거할 파일을 선택하세요.");
        return;
    }

    QString confirmMsg = targets.size() == 1
                             ? QString("'%1'\n(%2)\n목록에서 제거하시겠습니까?")
                                   .arg(targets.first()->text())
                                   .arg(targets.first()->toolTip())
                             : QString("%1개 파일을 목록에서 제거하시겠습니까?").arg(targets.size());

    if (QMessageBox::question(this, "파일 제거", confirmMsg,
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) return;

    for (auto *item : targets) {
        QString path = item->toolTip();
        stopWatching(path);          // ★ 감시 중이면 먼저 해제
        m_fileColors.remove(path);
        delete m_fileListWidget->takeItem(m_fileListWidget->row(item));
    }

    m_logTableWidget->setRowCount(0);
    m_allEntries.clear();
    m_parseBtnAction->setVisible(false);
    m_currentFile.clear();
    statusBar()->showMessage("파일이 제거되었습니다.");
    updateWatchUI();  // ★ 상태바 갱신
}

// ── 파일 선택 시 ───────────────────────────────────────────
void MainWindow::onFileSelected(QListWidgetItem *item)
{
    m_currentFile = item->toolTip();
    QFileInfo fi(m_currentFile);

    // 캐시 있으면 크기 무관하게 즉시 표시
    if (m_entryCache.contains(m_currentFile)) {
        m_parseBtnAction->setVisible(false);
        parseAndDisplay(m_currentFile); // 내부에서 캐시 분기 처리
        return;
    }

    if (fi.size() <= AUTO_PARSE_LIMIT) {
        m_parseBtnAction->setVisible(false);
        parseAndDisplay(m_currentFile);
    } else {
        m_parseBtnAction->setVisible(true);
        m_logTableWidget->setRowCount(0);
        m_allEntries.clear();
        statusBar()->showMessage(
            QString("대용량 파일 (%1 MB) — [▶ 파싱 시작] 버튼을 누르세요.")
                .arg(fi.size() / 1024.0 / 1024.0, 0, 'f', 1));
    }
}

// ── 수동 파싱 버튼 ─────────────────────────────────────────
void MainWindow::onParseFile()
{
    if (m_currentFile.isEmpty()) return;
    statusBar()->showMessage("파싱 중...");
    parseAndDisplay(m_currentFile);
    m_parseBtnAction->setVisible(false);
}

// ── 실제 파싱 + 테이블 표시 ────────────────────────────────
void MainWindow::parseAndDisplay(const QString &filePath)
{
    // 캐시에 있으면 재파싱 없이 바로 표시
    if (m_entryCache.contains(filePath)) {
        const ParseResult &cached = m_entryCache[filePath]; // 복사 없이 참조
        m_allEntries = cached.entries;                       // 이건 필요 (필터용)

        // minDt/maxDt 계산 없이 캐시된 값 바로 사용
        if (cached.minDt.isValid() && cached.maxDt.isValid()) {
            m_dateFrom->blockSignals(true); m_dateTo->blockSignals(true);
            m_timeFrom->blockSignals(true); m_timeTo->blockSignals(true);
            m_dateFrom->setDate(cached.minDt.date()); m_dateTo->setDate(cached.maxDt.date());
            m_timeFrom->setTime(cached.minDt.time()); m_timeTo->setTime(cached.maxDt.time());
            m_dateFrom->blockSignals(false); m_dateTo->blockSignals(false);
            m_timeFrom->blockSignals(false); m_timeTo->blockSignals(false);
        }

        // 필터 초기화 후 populateTable 직접 호출 (applyFilters 대신)
        m_chkError->blockSignals(true);  m_chkError->setChecked(true);  m_chkError->blockSignals(false);
        m_chkWarn->blockSignals(true);   m_chkWarn->setChecked(true);   m_chkWarn->blockSignals(false);
        m_chkInfo->blockSignals(true);   m_chkInfo->setChecked(true);   m_chkInfo->blockSignals(false);
        m_chkNoise->blockSignals(true);  m_chkNoise->setChecked(true);  m_chkNoise->blockSignals(false);
        m_sortCombo->blockSignals(true); m_sortCombo->setCurrentIndex(0); m_sortCombo->blockSignals(false);
        m_searchEdit->blockSignals(true); m_searchEdit->clear(); m_searchEdit->blockSignals(false);

        populateTable(cached.entries);

        QFileInfo fi(filePath);
        statusBar()->showMessage(
            QString("%1 — 총 %2줄 (캐시)").arg(fi.fileName()).arg(cached.entries.size()));
        setWindowTitle("TellMeLog — " + fi.fileName());
        return;
    }

    // 백그라운드 파싱
    statusBar()->showMessage(QString("파싱 중... (%1)").arg(QFileInfo(filePath).fileName()));
    m_parseBtnAction->setVisible(false);
    statusBar()->showMessage(QString("파싱 중... (%1)").arg(QFileInfo(filePath).fileName()));

    auto *watcher = new QFutureWatcher<ParseResult>(this);

    connect(watcher, &QFutureWatcher<ParseResult>::finished, this,
            [this, filePath, watcher]() {
                ParseResult result = watcher->result();
                watcher->deleteLater();

                m_entryCache[filePath] = result;          // ParseResult 통째로 저장
                m_allEntries = result.entries;

                if (m_watchedFiles.contains(filePath))
                    m_fileTailPos[filePath] = QFileInfo(filePath).size();

                // 필터 전체 초기화
                m_chkError->blockSignals(true);  m_chkError->setChecked(true);  m_chkError->blockSignals(false);
                m_chkWarn->blockSignals(true);   m_chkWarn->setChecked(true);   m_chkWarn->blockSignals(false);
                m_chkInfo->blockSignals(true);   m_chkInfo->setChecked(true);   m_chkInfo->blockSignals(false);
                m_chkNoise->blockSignals(true);  m_chkNoise->setChecked(true);  m_chkNoise->blockSignals(false);
                m_sortCombo->blockSignals(true); m_sortCombo->setCurrentIndex(0); m_sortCombo->blockSignals(false);
                m_searchEdit->blockSignals(true); m_searchEdit->clear(); m_searchEdit->blockSignals(false);

                // 날짜/시간 범위 설정 (백그라운드에서 계산된 값 사용)
                if (result.minDt.isValid() && result.maxDt.isValid()) {
                    m_dateFrom->blockSignals(true); m_dateTo->blockSignals(true);
                    m_timeFrom->blockSignals(true); m_timeTo->blockSignals(true);
                    m_dateFrom->setDate(result.minDt.date()); m_dateTo->setDate(result.maxDt.date());
                    m_timeFrom->setTime(result.minDt.time()); m_timeTo->setTime(result.maxDt.time());
                    m_dateFrom->blockSignals(false); m_dateTo->blockSignals(false);
                    m_timeFrom->blockSignals(false); m_timeTo->blockSignals(false);
                }

                m_progressBar->setValue(100);
                m_progressBar->setVisible(false);

                double elapsed = m_parseTimer.elapsed() / 1000.0;
                statusBar()->showMessage(
                    QString("파싱 완료 — 총 %1줄 (노이즈 %2줄) | 소요 시간: %3초")
                        .arg(result.entries.size())
                        .arg(result.noiseCount)
                        .arg(elapsed, 0, 'f', 2)
                    );
                setWindowTitle("TellMeLog — " + QFileInfo(filePath).fileName());
                m_parseBtnAction->setVisible(false);
            });

    m_parseTimer.start();
    // QtConcurrent::run 안에서 무거운 작업 모두 처리
    watcher->setFuture(QtConcurrent::run([this, filePath]() {
        ParseResult result;

        result.entries = m_parser.parse(filePath,
                                        [this](int pct) {
                                            QMetaObject::invokeMethod(this, [this, pct]() {
                                                m_progressBar->setValue(pct);
                                            }, Qt::QueuedConnection);
                                        },
                                        [this](const QVector<LogEntry> &batch) {
                                            QMetaObject::invokeMethod(this, [this, batch]() {
                                                m_logTableWidget->setUpdatesEnabled(false);
                                                for (const LogEntry &entry : batch) {
                                                    int row = m_logTableWidget->rowCount();
                                                    m_logTableWidget->insertRow(row);
                                                    m_logTableWidget->setItem(row, 0, new QTableWidgetItem(entry.date));
                                                    m_logTableWidget->setItem(row, 1, new QTableWidgetItem(entry.timestamp));
                                                    m_logTableWidget->setItem(row, 2, new QTableWidgetItem(entry.level));
                                                    m_logTableWidget->setItem(row, 3, new QTableWidgetItem(entry.module));
                                                    m_logTableWidget->setItem(row, 4, new QTableWidgetItem(entry.message));

                                                    QColor rowColor;
                                                    if (!entry.parsed)               rowColor = QColor("#f0f0f0");
                                                    else if (entry.level == "ERROR") rowColor = QColor("#ffe0e0");
                                                    else if (entry.level == "WARN")  rowColor = QColor("#fff4cc");
                                                    if (rowColor.isValid()) {
                                                        for (int col = 0; col < 5; ++col)
                                                            if (auto *cell = m_logTableWidget->item(row, col))
                                                                cell->setBackground(rowColor);
                                                    }
                                                }
                                                m_logTableWidget->setUpdatesEnabled(true);
                                            }, Qt::QueuedConnection);
                                        });

        // 백그라운드에서 무거운 후처리
        auto toDateTime = [](const LogEntry &e) -> QDateTime {
            return e.sortKey;  // 이미 계산된 값 그대로 반환
        };

        static const QRegularExpression reDMY(R"(^(\d{2})/(\d{2})/(\d{4})$)");
        for (LogEntry &e : result.entries) {
            e.sourceFile = filePath;
            if (!e.parsed) { ++result.noiseCount; continue; }

            // 날짜 정규화
            QString dateNorm = e.date;
            QRegularExpressionMatch dm = reDMY.match(dateNorm);
            if (dm.hasMatch())
                dateNorm = dm.captured(3) + "-" + dm.captured(2) + "-" + dm.captured(1);
            else if (dateNorm.contains('/'))
                dateNorm.replace('/', '-');

            // 직접 파싱 (fromString 제거)
            const QString &t = e.timestamp;
            if (dateNorm.size() >= 10 && t.size() >= 8) {
                int y  = QStringView(dateNorm).mid(0, 4).toInt();
                int mo = QStringView(dateNorm).mid(5, 2).toInt();
                int dy = QStringView(dateNorm).mid(8, 2).toInt();
                int h  = QStringView(t).mid(0, 2).toInt();
                int mi = QStringView(t).mid(3, 2).toInt();
                int s  = QStringView(t).mid(6, 2).toInt();
                int ms = (t.size() > 9) ? QStringView(t).mid(9).left(3).toInt() : 0;
                e.sortKey = QDateTime(QDate(y, mo, dy), QTime(h, mi, s, ms));
            }
        }

        // 첫 번째 parsed 엔트리 → minDt
        for (const LogEntry &e : result.entries) {
            if (!e.parsed) continue;
            result.minDt = toDateTime(e);
            if (result.minDt.isValid()) break;
        }

        // 마지막 parsed 엔트리 → maxDt
        for (int i = result.entries.size() - 1; i >= 0; --i) {
            if (!result.entries[i].parsed) continue;
            result.maxDt = toDateTime(result.entries[i]);
            if (result.maxDt.isValid()) break;
        }

        return result;
    }));
}

// ── 다중 파일 병합 파싱 ────────────────────────────────────
void MainWindow::mergeAndDisplay(const QStringList &filePaths)
{
    m_allEntries.clear();
    int totalNoise = 0;

    for (const QString &path : filePaths) {
        QVector<LogEntry> entries = m_parser.parse(path);
        totalNoise += m_parser.noiseCount();

        for (LogEntry &e : entries)
            e.sourceFile = path;

        m_allEntries.append(entries);
    }

    QDateTime minDt, maxDt;
    for (const LogEntry &e : m_allEntries) {
        if (!e.parsed) continue;
        QString dtStr = e.date + " " + e.timestamp;
        QDateTime dt = QDateTime::fromString(dtStr, "yyyy-MM-dd HH:mm:ss.zzz");
        if (!dt.isValid())
            dt = QDateTime::fromString(dtStr, "yyyy-MM-dd HH:mm:ss");
        if (!dt.isValid()) continue;
        if (!minDt.isValid() || dt < minDt) minDt = dt;
        if (!maxDt.isValid() || dt > maxDt) maxDt = dt;
    }

    if (minDt.isValid() && maxDt.isValid()) {
        m_dateFrom->blockSignals(true); m_dateTo->blockSignals(true);
        m_timeFrom->blockSignals(true); m_timeTo->blockSignals(true);
        m_dateFrom->setDate(minDt.date()); m_dateTo->setDate(maxDt.date());
        m_timeFrom->setTime(minDt.time()); m_timeTo->setTime(maxDt.time());
        m_dateFrom->blockSignals(false); m_dateTo->blockSignals(false);
        m_timeFrom->blockSignals(false); m_timeTo->blockSignals(false);
    }

    statusBar()->showMessage(
        QString("병합: %1개 파일 — 총 %2줄 (노이즈: %3줄)")
            .arg(filePaths.size())
            .arg(m_allEntries.size())
            .arg(totalNoise));
    setWindowTitle(QString("TellMeLog — %1개 파일 병합").arg(filePaths.size()));
}

// ── 필터/정렬 적용 ─────────────────────────────────────────
void MainWindow::applyFilters()
{
    if (m_allEntries.isEmpty()) return;

    bool showError = m_chkError->isChecked();
    bool showWarn  = m_chkWarn->isChecked();
    bool showInfo  = m_chkInfo->isChecked();
    bool showNoise = m_chkNoise->isChecked();
    QString sortMode = m_sortCombo->currentData().toString();

    QString keyword     = m_searchEdit->text().trimmed();
    QString searchScope = m_searchScopeCombo->currentData().toString();

    QDateTime dtFrom(m_dateFrom->date(), m_timeFrom->time());
    QDateTime dtTo  (m_dateTo->date(),   m_timeTo->time());

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
            if (!showInfo) continue;
        }

        // 날짜/시간 필터 — sortKey 직접 비교 (fromString 제거)
        if (e.parsed && e.sortKey.isValid()) {
            if (e.sortKey < dtFrom || e.sortKey > dtTo) continue;
        }

        // 검색 필터
        if (!keyword.isEmpty()) {
            bool hit = false;
            if (searchScope == "module") {
                hit = e.module.contains(keyword, Qt::CaseInsensitive);
            } else if (searchScope == "message") {
                hit = e.message.contains(keyword, Qt::CaseInsensitive);
            } else {
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

    // 정렬 — sortKey 직접 비교 (toDateTime 람다 제거)
    if (sortMode != "original") {
        std::sort(filtered.begin(), filtered.end(),
                  [&](const LogEntry &a, const LogEntry &b) {
                      if (!a.sortKey.isValid() && !b.sortKey.isValid()) return false;
                      if (!a.sortKey.isValid()) return false;
                      if (!b.sortKey.isValid()) return true;
                      return (sortMode == "desc") ? a.sortKey > b.sortKey
                                                  : a.sortKey < b.sortKey;
                  });
    }

    populateTable(filtered);

    QString statusMsg = QString("표시: %1줄 / 전체: %2줄")
                            .arg(filtered.size())
                            .arg(m_allEntries.size());
    if (!keyword.isEmpty())
        statusMsg += QString("  |  🔍 \"%1\" (%2 범위)").arg(keyword).arg(m_searchScopeCombo->currentText());
    if (!m_watchedFiles.isEmpty())
        statusMsg += QString("  |  👁 감시 중: %1개 파일").arg(m_watchedFiles.size());

    statusBar()->showMessage(statusMsg);
}

// ── 테이블 채우기 ──────────────────────────────────────────
void MainWindow::populateTable(const QVector<LogEntry> &entries)
{
    int checkedCount = 0;
    for (int i = 0; i < m_fileListWidget->count(); ++i) {
        if (m_fileListWidget->item(i)->checkState() == Qt::Checked)
            ++checkedCount;
    }
    bool mergeMode = (checkedCount > 1);

    m_logTableWidget->setSortingEnabled(false);
    m_logTableWidget->setUpdatesEnabled(false);
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
        } else if (mergeMode && !entry.sourceFile.isEmpty()) {
            rowColor = m_fileColors.value(entry.sourceFile, QColor("#FFFFFF"));
        }
        if (rowColor.isValid()) {
            for (int col = 0; col < 5; ++col)
                if (auto *cell = m_logTableWidget->item(row, col))
                    cell->setBackground(rowColor);
        }
    }

    m_logTableWidget->setUpdatesEnabled(true);
    m_logTableWidget->setSortingEnabled(false); // 자동 정렬은 계속 비활성화
}

// ── CSV 내보내기 ───────────────────────────────────────────
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

// ════════════════════════════════════════════════════════════
// ★ 실시간 감시 구현
// ════════════════════════════════════════════════════════════

// ── 감시 시작/중지 토글 ────────────────────────────────────
void MainWindow::onToggleWatch()
{
    // 현재 표시 중인 파일(들) 대상으로 토글
    // 체크된 파일이 있으면 체크된 파일들, 없으면 m_currentFile
    QStringList targets;
    for (int i = 0; i < m_fileListWidget->count(); ++i) {
        auto *it = m_fileListWidget->item(i);
        if (it->checkState() == Qt::Checked)
            targets << it->toolTip();
    }
    if (targets.isEmpty() && !m_currentFile.isEmpty())
        targets << m_currentFile;

    if (targets.isEmpty()) {
        QMessageBox::information(this, "알림", "감시할 파일을 먼저 선택하세요.");
        return;
    }

    // 대상 파일들이 이미 모두 감시 중이면 → 전체 중지
    // 하나라도 감시 안 하고 있으면 → 전체 시작
    bool allWatched = true;
    for (const QString &p : targets) {
        if (!m_watchedFiles.contains(p)) { allWatched = false; break; }
    }

    if (allWatched) {
        for (const QString &p : targets)
            stopWatching(p);
    } else {
        for (const QString &p : targets)
            startWatching(p);
    }

    updateWatchUI();
}

// ── 단일 파일 감시 시작 ────────────────────────────────────
void MainWindow::startWatching(const QString &filePath)
{
    if (m_watchedFiles.contains(filePath)) return;

    // CSV 감시 불가 안내 (헤더 재해석 문제)
    if (QFileInfo(filePath).suffix().toLower() == "csv") {
        QMessageBox::information(this, "알림",
                                 "CSV 파일은 실시간 감시를 지원하지 않습니다.\n(.log / .txt 파일만 지원)");
        return;
    }

    // 현재 파일 끝 위치를 tail 시작점으로 저장
    m_fileTailPos[filePath] = QFileInfo(filePath).size();
    m_watchedFiles.insert(filePath);
    m_watcher->addPath(filePath);

    // 파일 목록 갱신 (🟢 아이콘 반영)
    m_fileListWidget->viewport()->update();
}

// ── 단일 파일 감시 중지 ────────────────────────────────────
void MainWindow::stopWatching(const QString &filePath)
{
    if (!m_watchedFiles.contains(filePath)) return;

    m_watchedFiles.remove(filePath);
    m_fileTailPos.remove(filePath);
    m_watcher->removePath(filePath);

    m_fileListWidget->viewport()->update();
}

// ── 전체 감시 중지 ─────────────────────────────────────────
void MainWindow::stopAllWatching()
{
    for (const QString &p : m_watchedFiles)
        m_watcher->removePath(p);
    m_watchedFiles.clear();
    m_fileTailPos.clear();
    m_fileListWidget->viewport()->update();
}

// ── 감시 버튼/상태바 UI 갱신 ───────────────────────────────
void MainWindow::updateWatchUI()
{
    bool anyWatching = !m_watchedFiles.isEmpty();

    if (anyWatching) {
        m_watchBtn->setText(QString("👁 감시 중 (%1)").arg(m_watchedFiles.size()));
        m_watchBtn->setStyleSheet(m_watchBtn->property("activeStyle").toString());
        m_watchBtn->setToolTip("클릭하면 감시를 중지합니다");
    } else {
        m_watchBtn->setText("👁 감시 시작");
        m_watchBtn->setStyleSheet(m_watchBtn->property("inactiveStyle").toString());
        m_watchBtn->setToolTip("현재 파일의 변경을 실시간으로 감시합니다");
    }

    // 상태바 갱신 (applyFilters 재호출 없이 직접 갱신)
    // applyFilters가 이미 상태바 메시지에 감시 상태를 반영하므로
    // 빈 m_allEntries일 때만 별도 처리
    if (m_allEntries.isEmpty()) {
        if (anyWatching) {
            statusBar()->showMessage(
                QString("👁 감시 중: %1개 파일").arg(m_watchedFiles.size()));
        } else {
            statusBar()->showMessage("파일을 추가하세요.");
        }
    } else {
        applyFilters(); // 상태바에 감시 상태 포함시키기 위해 재호출
    }
}

// ── 파일 변경 감지 시 ─────────────────────────────────────
// QFileSystemWatcher::fileChanged 시그널 수신
// 주의: 일부 에디터는 파일을 delete→recreate 방식으로 저장하므로
//        파일이 사라졌다가 다시 생기면 감시자에서 제거됨 → 재등록 필요
void MainWindow::onFileChanged(const QString &path)
{
    // 파일이 삭제/이동된 경우 (일부 에디터의 atomic save 동작)
    if (!QFileInfo::exists(path)) {
        // 감시자에서 자동 제거되므로 내부 상태만 동기화
        // 잠시 후 재등록 시도 (500ms 대기)
        QTimer::singleShot(500, this, [this, path]() {
            if (m_watchedFiles.contains(path) && QFileInfo::exists(path)) {
                m_watcher->addPath(path);
                // 파일이 재생성됐으므로 전체 재파싱
                qint64 dummy = 0;
                QVector<LogEntry> newEntries = m_parser.parseTail(path, 0, dummy);
                m_fileTailPos[path] = dummy;

                for (LogEntry &e : newEntries) e.sourceFile = path;

                // m_allEntries 중 이 파일 항목 교체
                m_allEntries.erase(
                    std::remove_if(m_allEntries.begin(), m_allEntries.end(),
                                   [&path](const LogEntry &e) { return e.sourceFile == path; }),
                    m_allEntries.end());
                m_allEntries.append(newEntries);
                applyFilters();
            }
        });
        return;
    }

    // 정상 append: startOffset부터 새 줄만 읽기
    qint64 startOffset = m_fileTailPos.value(path, 0);
    qint64 endOffset   = startOffset;

    QVector<LogEntry> newEntries = m_parser.parseTail(path, startOffset, endOffset);

    if (newEntries.isEmpty()) {
        // offset 갱신 (파일이 truncate됐을 수도 있음)
        m_fileTailPos[path] = endOffset;
        return;
    }

    m_fileTailPos[path] = endOffset;

    for (LogEntry &e : newEntries)
        e.sourceFile = path;

    appendNewEntries(path, newEntries);
}

// ── 새 항목을 m_allEntries에 추가 + 테이블에 반영 ──────────
void MainWindow::appendNewEntries(const QString &filePath,
                                  const QVector<LogEntry> &newEntries)
{
    // m_allEntries에 추가
    m_allEntries.append(newEntries);

    // 현재 표시 중인 파일과 관련 없는 항목이면 m_allEntries에만 추가 (테이블 갱신 안 함)
    // 현재 단독 뷰: m_currentFile 기준
    // 병합 뷰: 체크된 파일 기준
    QStringList visibleFiles;
    int checkedCount = 0;
    for (int i = 0; i < m_fileListWidget->count(); ++i) {
        auto *it = m_fileListWidget->item(i);
        if (it->checkState() == Qt::Checked) {
            visibleFiles << it->toolTip();
            ++checkedCount;
        }
    }
    if (checkedCount == 0 && !m_currentFile.isEmpty())
        visibleFiles << m_currentFile;

    if (!visibleFiles.contains(filePath))
        return; // 현재 보이지 않는 파일 → 테이블 갱신 없이 데이터만 추가

    // 현재 필터 조건을 통과하는 새 항목만 테이블 맨 아래에 추가
    bool showError = m_chkError->isChecked();
    bool showWarn  = m_chkWarn->isChecked();
    bool showInfo  = m_chkInfo->isChecked();
    bool showNoise = m_chkNoise->isChecked();
    QString keyword     = m_searchEdit->text().trimmed();
    QString searchScope = m_searchScopeCombo->currentData().toString();
    QDateTime dtFrom(m_dateFrom->date(), m_timeFrom->time());
    QDateTime dtTo  (m_dateTo->date(),   m_timeTo->time());
    QString sortMode = m_sortCombo->currentData().toString();

    // 정렬 모드가 내림차순이면 테이블 전체 갱신이 필요 (새 항목이 위로 올라가야 하므로)
    // 원본/오름차순이면 테이블 끝에만 추가
    if (sortMode == "desc") {
        applyFilters();
        return;
    }

    int insertedFrom = m_logTableWidget->rowCount();
    bool mergeMode = (checkedCount > 1);

    for (const LogEntry &e : newEntries) {
        // 레벨 필터
        if (!e.parsed) {
            if (!showNoise) continue;
        } else if (e.level == "ERROR") {
            if (!showError) continue;
        } else if (e.level == "WARN") {
            if (!showWarn) continue;
        } else {
            if (!showInfo) continue;
        }

        // 날짜/시간 필터
        if (e.parsed) {
            QString dtStr = e.date + " " + e.timestamp;
            QDateTime dt = QDateTime::fromString(dtStr, "yyyy-MM-dd HH:mm:ss.zzz");
            if (!dt.isValid()) dt = QDateTime::fromString(dtStr, "yyyy-MM-dd HH:mm:ss");
            if (dt.isValid() && (dt < dtFrom || dt > dtTo)) continue;
        }

        // 검색 필터
        if (!keyword.isEmpty()) {
            bool hit = false;
            if (searchScope == "module")
                hit = e.module.contains(keyword, Qt::CaseInsensitive);
            else if (searchScope == "message")
                hit = e.message.contains(keyword, Qt::CaseInsensitive);
            else
                hit = e.module.contains(keyword, Qt::CaseInsensitive)
                      || e.message.contains(keyword, Qt::CaseInsensitive)
                      || e.level.contains(keyword, Qt::CaseInsensitive)
                      || e.date.contains(keyword, Qt::CaseInsensitive)
                      || e.timestamp.contains(keyword, Qt::CaseInsensitive);
            if (!hit) continue;
        }

        // 테이블에 행 추가
        int row = m_logTableWidget->rowCount();
        m_logTableWidget->insertRow(row);
        m_logTableWidget->setItem(row, 0, new QTableWidgetItem(e.date));
        m_logTableWidget->setItem(row, 1, new QTableWidgetItem(e.timestamp));
        m_logTableWidget->setItem(row, 2, new QTableWidgetItem(e.level));
        m_logTableWidget->setItem(row, 3, new QTableWidgetItem(e.module));
        m_logTableWidget->setItem(row, 4, new QTableWidgetItem(e.message));

        QColor rowColor;
        if (!e.parsed) rowColor = QColor("#f0f0f0");
        else if (e.level == "ERROR") rowColor = QColor("#ffe0e0");
        else if (e.level == "WARN")  rowColor = QColor("#fff4cc");
        else if (mergeMode && !e.sourceFile.isEmpty())
            rowColor = m_fileColors.value(e.sourceFile, QColor("#FFFFFF"));

        if (rowColor.isValid()) {
            for (int col = 0; col < 5; ++col)
                if (auto *cell = m_logTableWidget->item(row, col))
                    cell->setBackground(rowColor);
        }
    }

    int insertedTo = m_logTableWidget->rowCount() - 1;

    if (insertedTo >= insertedFrom) {
        // 새 행 하이라이트 + 스크롤
        highlightNewRows(insertedFrom, insertedTo);
        m_logTableWidget->scrollToItem(
            m_logTableWidget->item(insertedTo, 0), QAbstractItemView::PositionAtBottom);
    }

    // 상태바 갱신
    QString statusMsg = QString("표시: %1줄 / 전체: %2줄  |  ✨ 새 항목 +%3줄")
                            .arg(m_logTableWidget->rowCount())
                            .arg(m_allEntries.size())
                            .arg(newEntries.size());
    if (!m_watchedFiles.isEmpty())
        statusMsg += QString("  |  👁 감시 중: %1개 파일").arg(m_watchedFiles.size());
    statusBar()->showMessage(statusMsg);
}

// ── 신규 행 노란 하이라이트 페이드 시작 ────────────────────
void MainWindow::highlightNewRows(int fromRow, int toRow)
{
    if (fromRow > toRow) return;

    // 먼저 노란색으로 즉시 칠함
    for (int row = fromRow; row <= toRow; ++row) {
        for (int col = 0; col < 5; ++col) {
            if (auto *cell = m_logTableWidget->item(row, col))
                cell->setBackground(QColor(255, 255, 180)); // 밝은 노랑
        }
    }

    // 페이드 범위 등록 후 타이머 시작
    m_pendingHighlightRanges.append({fromRow, toRow});

    if (!m_highlightTimer->isActive()) {
        m_highlightStep = 0;
        m_highlightTimer->start();
    }
}