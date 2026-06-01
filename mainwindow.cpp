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
#include <QLineEdit>        // ★ 추가
#include <QPainter>         // 색상 뱃지
#include <QStyledItemDelegate>
#include <QApplication>
#include <QMouseEvent>
#include <QEvent>

// ── 파일 목록 커스텀 델리게이트 ─────────────────────────────
// 역할:
//   1) 아이템 배경을 파일 고유 색으로 채움 (선택/호버도 해당 색 기준)
//   2) 왼쪽에 체크박스를 직접 그림 (Qt 체크박스 스타일 사용)
//   3) 체크박스 영역 / 텍스트 영역을 분리해서 클릭 구분에 사용
class FileListDelegate : public QStyledItemDelegate
{
public:
    static constexpr int CHECKBOX_WIDTH = 26; // 체크박스 영역 너비 (px), 여백 포함

    explicit FileListDelegate(const QMap<QString, QColor> *colorMap, QObject *parent = nullptr)
        : QStyledItemDelegate(parent), m_colorMap(colorMap) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QString filePath = index.data(Qt::ToolTipRole).toString();
        bool checked = (index.data(Qt::CheckStateRole).toInt() == Qt::Checked);

        // 체크됐을 때만 파일 고유 색상, 기본은 흰색
        QColor baseColor = checked
                               ? m_colorMap->value(filePath, QColor("#FFFFFF"))
                               : QColor("#FFFFFF");

        // 호버: 연한 회색 / 선택(하이라이트): baseColor 기준으로 진하게
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

        // 2) 체크박스 그리기 (시스템 스타일 사용)
        QStyleOptionButton cbOpt;
        cbOpt.rect = checkBoxRect(option.rect);
        cbOpt.state = QStyle::State_Enabled;
        cbOpt.state |= checked ? QStyle::State_On : QStyle::State_Off;
        QApplication::style()->drawPrimitive(QStyle::PE_IndicatorCheckBox, &cbOpt, painter);

        // 3) 텍스트 (체크박스와 4px 여백)
        QRect textRect = option.rect.adjusted(CHECKBOX_WIDTH + 6, 0, -4, 0);
        QString text = index.data(Qt::DisplayRole).toString();
        painter->setPen(QColor("#000000"));
        painter->setFont(option.font);
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                          option.fontMetrics.elidedText(text, Qt::ElideRight, textRect.width()));

        painter->restore();
    }

    // 체크박스 영역 반환 (MainWindow에서 클릭 구분에도 사용)
    static QRect checkBoxRect(const QRect &itemRect)
    {
        int cbSize = 16;
        int x = itemRect.left() + 5;
        int y = itemRect.top() + (itemRect.height() - cbSize) / 2;
        return QRect(x, y, cbSize, cbSize);
    }

private:
    const QMap<QString, QColor> *m_colorMap;
};

// ─────────────────────────────────────────────────────────────

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
    m_fileListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fileListWidget->setMouseTracking(true);  // 호버 감지에 필요
    // ★ 커스텀 델리게이트 적용 (파일 색 배경 + 체크박스)
    m_fileListWidget->setItemDelegate(new FileListDelegate(&m_fileColors, m_fileListWidget));
    // ★ viewport에 이벤트 필터 설치 — 마우스 클릭 위치로 체크박스/텍스트 영역 직접 구분
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
    splitter->setSizes({240, 960});  // ★ 좌측 패널 초기 너비 240px

    setCentralWidget(splitter);
}

// ── setupFilterBar 별도 함수로 빼지 않음 (setupUI 내에 통합) ──
void MainWindow::setupFilterBar() {} // 빈 구현 (헤더 선언 유지용)

// ── 파일 목록 클릭 처리 (체크박스 / 텍스트 영역 구분) ────────
// itemPressed 대신 eventFilter를 사용하는 이유:
//   - itemPressed는 발생 시점에 QCursor::pos()가 부정확할 수 있음
//   - setCheckState()가 내부적으로 itemChanged를 발생시켜 이중 처리 발생 가능
//   - eventFilter는 MouseButtonPress 시점의 정확한 pos()를 직접 사용
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
                // ── 체크박스 영역 클릭: 토글 후 체크된 파일 목록 갱신 ──
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
                    parseAndDisplay(m_currentFile);
                } else {
                    mergeAndDisplay(checkedPaths);
                }
                return true;  // 이벤트 소비 (QListWidget 기본 선택 동작 방지)

            } else {
                // ── 텍스트/행 영역 클릭: 이 파일만 단독 표시 ──
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
    // 파일별 배경색 팔레트 (ERROR/WARN 색과 겹치지 않는 파스텔 계열)
    static const QList<QColor> FILE_PALETTE = {
        QColor("#FFFFFF"),   // 파일 1: 흰색 (기본)
        QColor("#EBF5FB"),   // 파일 2: 하늘
        QColor("#E8F8F5"),   // 파일 3: 민트
        QColor("#F5EEF8"),   // 파일 4: 라벤더
        QColor("#FEF9E7"),   // 파일 5: 연노랑
        QColor("#FDFEFE"),   // 파일 6: 연회색
    };

    QStringList files = QFileDialog::getOpenFileNames(
        this, "로그 파일 선택", "",
        "로그/CSV 파일 (*.log *.txt *.csv);;모든 파일 (*)");

    for (const QString &path : files) {
        // 전체 경로로 중복 체크
        bool alreadyExists = false;
        for (int i = 0; i < m_fileListWidget->count(); ++i) {
            if (m_fileListWidget->item(i)->toolTip() == path) {
                alreadyExists = true;
                break;
            }
        }
        if (alreadyExists) continue;

        // 색상 배정 (현재 파일 수 기준 순환)
        int colorIdx = m_fileColors.size() % FILE_PALETTE.size();
        QColor assignedColor = FILE_PALETTE[colorIdx];
        m_fileColors[path] = assignedColor;

        // ★ 체크박스 방식 — 아이콘 뱃지 없이 아이템 생성
        QListWidgetItem *item = new QListWidgetItem(QFileInfo(path).fileName());
        item->setToolTip(path);
        item->setCheckState(Qt::Unchecked);  // 체크박스 초기값: 해제
        m_fileListWidget->addItem(item);
    }
}

// ── 파일 제거 ────────────────────────────────────────────
void MainWindow::onRemoveFile()
{
    // 체크된 항목이 있으면 체크된 것들을 제거, 없으면 선택(하이라이트)된 항목 제거
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
        m_fileColors.remove(item->toolTip());
        delete m_fileListWidget->takeItem(m_fileListWidget->row(item));
    }

    m_logTableWidget->setRowCount(0);
    m_allEntries.clear();
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
    m_allEntries = m_parser.parse(filePath);

    // ★ 출처 파일 경로 기록
    for (LogEntry &e : m_allEntries)
        e.sourceFile = filePath;

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

// ── 다중 파일 병합 파싱 ★ ────────────────────────────────
void MainWindow::mergeAndDisplay(const QStringList &filePaths)
{
    m_allEntries.clear();
    int totalNoise = 0;

    for (const QString &path : filePaths) {
        QVector<LogEntry> entries = m_parser.parse(path);
        totalNoise += m_parser.noiseCount();

        // 각 엔트리에 출처 파일 경로 기록
        for (LogEntry &e : entries)
            e.sourceFile = path;

        m_allEntries.append(entries);
    }

    // 시간 범위 자동 감지 (병합된 전체 기준)
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

    applyFilters();

    statusBar()->showMessage(
        QString("병합: %1개 파일 — 총 %2줄 (노이즈: %3줄)")
            .arg(filePaths.size())
            .arg(m_allEntries.size())
            .arg(totalNoise));
    setWindowTitle(QString("TellMeLog — %1개 파일 병합").arg(filePaths.size()));
}


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
    // 체크된 파일이 2개 이상이면 병합 모드 → 파일 색상 적용
    int checkedCount = 0;
    for (int i = 0; i < m_fileListWidget->count(); ++i) {
        if (m_fileListWidget->item(i)->checkState() == Qt::Checked)
            ++checkedCount;
    }
    bool mergeMode = (checkedCount > 1);

    m_logTableWidget->setRowCount(0);

    for (const LogEntry &entry : entries) {
        int row = m_logTableWidget->rowCount();
        m_logTableWidget->insertRow(row);

        m_logTableWidget->setItem(row, 0, new QTableWidgetItem(entry.date));
        m_logTableWidget->setItem(row, 1, new QTableWidgetItem(entry.timestamp));
        m_logTableWidget->setItem(row, 2, new QTableWidgetItem(entry.level));
        m_logTableWidget->setItem(row, 3, new QTableWidgetItem(entry.module));
        m_logTableWidget->setItem(row, 4, new QTableWidgetItem(entry.message));

        // ★ 색상 우선순위: ERROR > WARN > 노이즈 > 파일 색상(병합 시)
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
            for (int col = 0; col < 5; ++col) {
                if (auto *cell = m_logTableWidget->item(row, col))
                    cell->setBackground(rowColor);
            }
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