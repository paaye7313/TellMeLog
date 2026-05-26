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

### 툴바 버튼 연결
- 파일 추가 → onAddFile()
- 파일 제거 → onRemoveFile()
- 파싱 시작 → onParseFile() (대용량 전용, 평소 hidden)
- 리포트 생성 → onGenerateReport() (미구현 stub)

## 다음 단계
- [x] CSV 내보내기
  - 툴바에 📥 CSV 내보내기 버튼 추가
  - 현재 테이블 데이터 → CSV 저장
  - BOM 포함 (엑셀 한글 깨짐 방지)
  - RFC 4180 준수 (쉼표/따옴표 이스케이프)
  - 기본 저장 경로: 원본 로그 파일과 같은 폴더
- [ ] 리포트 생성
- [ ] 실시간 로그 감시 (QFileSystemWatcher)