# TellMeLog 실시간 감시 GIF 데모용 스크립트
# watch_demo.log 에 로그를 2초 간격으로 추가합니다.
# 사용법: 이 파일을 watch_demo.log 와 같은 폴더에 놓고 실행

$logFile = Join-Path $PSScriptRoot "watch_demo.log"

$entries = @(
    "2024-03-15 09:00:03.101 [INFO]  [WatchDemo] New task received. ID: TASK-001",
    "2024-03-15 09:00:05.240 [INFO]  [WatchDemo] Processing started.",
    "2024-03-15 09:00:07.389 [WARN]  [WatchDemo] Response time slow. Elapsed: 850ms",
    "2024-03-15 09:00:09.512 [INFO]  [WatchDemo] Checkpoint reached. Progress: 50%",
    "2024-03-15 09:00:11.774 [ERROR] [WatchDemo] Unexpected state. Code: 0xA3. Retrying...",
    "2024-03-15 09:00:13.002 [WARN]  [WatchDemo] Retry attempt 1/3",
    "2024-03-15 09:00:15.210 [INFO]  [WatchDemo] Recovery successful.",
    "2024-03-15 09:00:17.445 [INFO]  [WatchDemo] Processing resumed.",
    "2024-03-15 09:00:19.881 [INFO]  [WatchDemo] Task complete. ID: TASK-001",
    "2024-03-15 09:00:22.003 [INFO]  [WatchDemo] Waiting for next task..."
)

Write-Host "=== TellMeLog 실시간 감시 데모 ===" -ForegroundColor Cyan
Write-Host "로그 파일: $logFile" -ForegroundColor Gray
Write-Host "2초 간격으로 로그를 추가합니다. 종료: Ctrl+C" -ForegroundColor Gray
Write-Host ""

foreach ($line in $entries) {
    Add-Content -Path $logFile -Value $line -Encoding UTF8
    Write-Host "추가됨: $line" -ForegroundColor Green
    Start-Sleep -Seconds 2
}

Write-Host ""
Write-Host "완료!" -ForegroundColor Cyan
