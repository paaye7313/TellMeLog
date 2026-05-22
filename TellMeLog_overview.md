## 프로젝트 개요
프로젝트명: TellMeLog (로그야뭐라고)
Qt/C++ 기반 로그 자동분석 데스크톱 툴 (포트폴리오용)
타겟 직무: 생산기술 / 장비 제어 / 엔지니어링 툴 / 테스트 자동화 소프트웨어 개발

## 기술 스택
- 언어: C++17
- 프레임워크: Qt 6.5 LTS (Qt Widgets)
- 주요 Qt 모듈: QFileSystemWatcher, QTableWidget, QFileDialog
- 파싱: std::regex
- 출력: CSV 저장, 리포트 생성

## UI 구조
- 좌측: 로그 파일 목록 패널
- 우측: 파싱된 로그 테이블
- 상단: 리포트 생성 버튼

## 개발 환경
- OS: Windows
- IDE: Qt Creator
- 컴파일러: MinGW 64-bit (또는 MSVC)
- Qt 버전: Qt 6.5 LTS

## 내 배경
- PyQt/PySide 개인프로젝트 경험 있음
- C++은 대학 수준, 현재 복습 중
- claude.ai 채팅으로 개발 진행 (Claude Code 미사용)

## 진행 방식
- 코드는 항상 전체 파일 단위로 제공
- 변경 시 어느 부분이 바뀌었는지 명시
- C++ 문법 중 PyQt와 다른 부분은 간단히 설명 추가

## 개발 환경 세팅 완료 (2025-05-21)
- Qt 버전: Qt 6.11 (6.5 LTS 대신 최신 버전으로 진행)
- 빌드 시스템: CMake (Qt 5 Compatibility 미적용)
- 컴파일러: MinGW 64-bit
- 버전 관리: Git 적용
- 프로젝트 경로: C:\paaye\TellMeLog
- 현재 상태: 기본 빈 윈도우 실행 확인 완료
- 다음 단계: UI 레이아웃 구성