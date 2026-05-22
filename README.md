# TellMeLog (로그야 뭐라고)

Qt/C++ 기반 로그 자동 분석 데스크톱 툴

---

## 소개

장비 제어 및 테스트 자동화 환경에서 생성되는 로그 파일을 빠르게 파싱하고 시각화하는 데스크톱 애플리케이션입니다.
포트폴리오 목적으로 제작되었으며, 생산기술 / 장비 제어 / 엔지니어링 툴 / 테스트 자동화 직무를 타겟으로 합니다.

---

## 주요 기능

- 로그 파일 다중 추가 및 목록 관리
- 자동/수동 파싱 분기 (1MB 기준 자동 파싱, 대용량은 수동 파싱)
- 다양한 타임스탬프 포맷 지원
  - 표준: `YYYY-MM-DD HH:MM:SS.mmm`
  - 슬래시: `YYYY/MM/DD` 또는 `DD/MM/YYYY`
  - ISO 8601: `YYYY-MM-DDTHH:MM:SS.mmm+HH:MM`
- 로그 레벨별 색상 구분 (ERROR / WARN / 노이즈)
- 파싱 결과 상태바 요약 (총 줄 수 / 노이즈 줄 수)

---

## 기술 스택

| 항목 | 내용 |
|---|---|
| 언어 | C++17 |
| 프레임워크 | Qt 6.11 (Qt Widgets) |
| 빌드 시스템 | CMake |
| 컴파일러 | MinGW 64-bit |
| 주요 모듈 | QRegularExpression, QTableWidget, QFileDialog, QFileSystemWatcher |
| OS | Windows |

---

## UI 구조

```
┌─────────────────────────────────────────────┐
│  [📄 파일 추가]  [🗑 파일 제거]   [📊 리포트 생성]  │
├──────────────┬──────────────────────────────┤
│ 📂 로그 파일  │  📋 파싱 결과                  │
│   목록        │  날짜 │ 시간 │ 레벨 │ 모듈 │ 메시지 │
│              │                              │
└──────────────┴──────────────────────────────┘
│ 상태바: 파싱 결과 요약                           │
└─────────────────────────────────────────────┘
```

---

## 빌드 방법

**요구사항**
- Qt 6.5 이상
- CMake 3.19 이상
- MinGW 64-bit 또는 MSVC

```bash
git clone https://github.com/paaye7313/TellMeLog.git
cd TellMeLog
cmake -B build
cmake --build build
```

또는 Qt Creator에서 `CMakeLists.txt`를 열어 바로 빌드할 수 있습니다.

---

## 테스트 로그 파일

`testdata/logs/` 디렉토리에 파서 테스트용 샘플 파일이 포함되어 있습니다.

| 파일 | 설명 |
|---|---|
| `test_normal.log` | 표준 포맷 정상 로그 |
| `test_mixed.log` | WARN/ERROR, 노이즈, 타임스탬프 혼재 케이스 |

---

## 개발 환경

- IDE: Qt Creator
- 버전 관리: Git

---

## 개발 현황

- [x] 개발 환경 세팅
- [x] UI 레이아웃 구성
- [x] 로그 파일 파싱 기능
- [ ] CSV 내보내기
- [ ] 리포트 생성
- [ ] 실시간 로그 감시 (QFileSystemWatcher)
