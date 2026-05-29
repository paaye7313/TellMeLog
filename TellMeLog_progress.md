## TellMeLog 개발 진행 상황

## 완료
- [x] 개발 환경 세팅
  - Qt 6.11 설치
  - Qt Creator 프로젝트 생성 (CMake, MinGW 64-bit)
  - Git 연동
  - 기본 빈 윈도우 실행 확인

- [x] UI 레이아웃 구성
  - QSplitter로 좌우 패널 분리 (비율 1:3)
  - QToolBar에 파일 추가 / 제거 / 리포트 생성 버튼
  - QListWidget (파일 목록), QTableWidget (파싱 결과)
  - 선택/호버 색상 개선 (파란 계열)
  - 파일 제거 시 확인 다이얼로그

- [x] 로그 파일 파싱 기능
  - LogParser 클래스 추가 (logparser.h / logparser.cpp)
  - QRegularExpression으로 3가지 타임스탬프 포맷 지원
    - 표준: YYYY-MM-DD HH:MM:SS.mmm
    - 슬래시: YYYY/MM/DD 또는 DD/MM/YYYY
    - ISO 8601: YYYY-MM-DDTHH:MM:SS.mmm+HH:MM
  - 1MB 기준 자동/수동 파싱 분기
  - 테이블 컬럼: 날짜 / 시간 / 레벨 / 모듈 / 메시지 (5컬럼)
  - ERROR(빨강) / WARN(노랑) / 노이즈(회색) 행 색상 구분
  - 상태바 파싱 결과 요약 (총 줄 수 / 노이즈 줄 수)
  - 테스트 로그 파일 생성 (testdata/logs/)

- [x] README 작성 및 GitHub 업로드

- [x] CSV 내보내기
  - 툴바에 📥 CSV 내보내기 버튼 추가
  - BOM 포함 UTF-8 저장 (엑셀 한글 깨짐 방지)
  - RFC 4180 준수 (쉼표/따옴표 이스케이프)
  - 기본 저장 경로: 원본 로그 파일과 같은 폴더

- [x] CSV 읽기
  - 파일 추가 필터에 *.csv 추가
  - 날짜 컬럼 자동 탐지 (헤더 유무 무관)
  - 5컬럼 초과 시 메시지 뒤에 이어붙임
  - 날짜 컬럼 없으면 노이즈 처리

- [x] 필터 바 추가
  - 테이블 위 2줄 구성의 필터 바 UI
  - 1줄: 레벨별 토글 체크박스 (ERROR/WARN/INFO/노이즈) + 정렬 콤보박스
  - 2줄: 날짜 범위 필터 (QDateEdit From~To) + 시간 범위 필터 (QTimeEdit From~To)
  - 파싱 시 시간 범위 자동 감지 → DateEdit/TimeEdit 초기값 자동 설정
  - 전체 초기화 버튼 (레벨·검색·정렬·날짜/시간 전부 리셋)
  - 정렬 모드: 원본 순서 (기본) / 시간 오름차순 / 시간 내림차순
  - DD/MM/YYYY 형식 정렬 버그 수정 (QDateTime으로 정규화 후 비교)
  - 재파싱 없이 show/hide 방식 (m_allEntries 전체 보관)
  - 파일 목록 파일명만 표시, 툴팁으로 전체 경로 확인
  - CSV 노이즈 판정 기준 추가 (날짜 형식 + 레벨 유효성 검증)

- [x] 검색 기능
  - 필터 바 1번째 줄 오른쪽 인라인 배치 (🔍 검색창 + 범위 콤보박스)
  - 검색 범위: 전체 / 모듈 / 메시지
  - 실시간 필터 (textChanged 시그널, 대소문자 구분 없음)
  - X 버튼으로 검색어 즉시 삭제 (setClearButtonEnabled)
  - 레벨·날짜·시간 필터와 AND 조건으로 합산
  - 상태바에 검색어 및 범위 표시
  - 전체 초기화 버튼으로 검색어·범위도 함께 리셋

## 현재 코드 구조 (파일 업로드 없이 파악용)

### LogEntry 구조체 (logparser.h)
```cpp
struct LogEntry {
    QString date;       // 날짜
    QString timestamp;  // 시간
    QString level;      // ERROR / WARN / INFO 등
    QString module;     // 모듈명
    QString message;    // 메시지
    bool    parsed;     // false = 노이즈 라인
};
```

### LogParser 클래스 (logparser.h/cpp)
- `QVector<LogEntry> parse(const QString &filePath)` : 파일 파싱 후 entries 반환
- `int noiseCount()` : 마지막 파싱의 노이즈 줄 수
- 내부적으로 3가지 regex 패턴 매칭 (Standard / Slash / ISO)

### MainWindow 주요 멤버 (mainwindow.h/cpp)
- `m_fileListWidget` : QListWidget, 파일 경로 문자열 그대로 저장
- `m_logTableWidget` : QTableWidget, 5컬럼 (날짜/시간/레벨/모듈/메시지)
- `m_parser` : LogParser 인스턴스 (멤버 변수)
- `m_currentFile` : 현재 선택된 파일 경로 (QString)
- `m_addFileBtn / m_removeFileBtn / m_parseBtn / m_reportBtn` : QPushButton*
- `parseAndDisplay(filePath)` → `m_parser.parse()` 호출 → `populateTable()` 호출
- `AUTO_PARSE_LIMIT` = 1MB, 초과 시 m_parseBtn 표시
- `m_allEntries` : QVector<LogEntry>, 전체 파싱 결과 보관 (필터용)
- `m_chkError / m_chkWarn / m_chkInfo / m_chkNoise` : 레벨 필터 체크박스
- `m_dateFrom / m_dateTo` : QDateEdit, 날짜 범위 필터
- `m_timeFrom / m_timeTo` : QTimeEdit, 시간 범위 필터
- `m_btnResetTime` : 전체 초기화 버튼 (레벨·검색·정렬·날짜/시간)
- `m_sortCombo` : QComboBox, 정렬 모드 (original/asc/desc)
- `m_searchEdit` : QLineEdit, 실시간 검색창
- `m_searchScopeCombo` : QComboBox, 검색 범위 (all/module/message)
- `applyFilters()` → 레벨/날짜/시간/정렬/검색 조건 적용 후 populateTable() 호출

### 툴바 버튼 연결
- 파일 추가 → onAddFile()
- 파일 제거 → onRemoveFile()
- 파싱 시작 → onParseFile() (대용량 전용, 평소 hidden)
- 리포트 생성 → onGenerateReport() (미구현 stub)

## 다음 단계

- [ ] 리포트 생성
- [ ] 실시간 로그 감시 (QFileSystemWatcher)