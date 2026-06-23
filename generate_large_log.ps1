# generate_large_log.ps1
# 사용법: .\generate_large_log.ps1 -TargetMB 10 -OutputFile "testdata\logs\large_10mb.log"

param(
    [int]$TargetMB = 10,
    [string]$OutputFile = "large_test.log"
)

$TargetBytes = $TargetMB * 1MB
$Levels   = @("INFO", "INFO", "INFO", "WARN", "ERROR")  # INFO 비중 높게
$Modules  = @("MotionCtrl", "VisionSys", "IOCtrl", "Scheduler", "CommLayer")
$Messages = @(
    "Axis move command sent: X={0:F3} Y={1:F3}",
    "Image captured: frame={0} exposure={1}ms",
    "Digital output set: port={0} value={1}",
    "Task scheduled: id={0} interval={1}ms",
    "Packet received: seq={0} size={1}bytes",
    "Temperature check passed: {0:F1}C",
    "Retry attempt {0} of {1}",
    "Buffer flush complete: {0} entries",
    "Connection established to {0}:{1}",
    "Calibration offset applied: {0:F4}"
)

$start = [datetime]::new(2025, 1, 1, 8, 0, 0)
$rng   = [System.Random]::new(42)  # 고정 시드 (재현성)
$lineCount = 0
$written   = 0

Write-Host "Generating ${TargetMB}MB log -> $OutputFile ..."

$sw = [System.IO.StreamWriter]::new($OutputFile, $false, [System.Text.Encoding]::UTF8)

try {
    while ($written -lt $TargetBytes) {
        $start    = $start.AddMilliseconds($rng.Next(1, 500))
        $level    = $Levels[$rng.Next($Levels.Length)]
        $module   = $Modules[$rng.Next($Modules.Length)]
        $msgTpl   = $Messages[$rng.Next($Messages.Length)]
        $a = $rng.Next(0, 9999)
        $b = $rng.Next(0, 9999)
        $msg = $msgTpl -f $a, $b

        $line = "{0:yyyy-MM-dd} {0:HH:mm:ss.fff} [{1}] [{2}] {3}" -f $start, $level, $module, $msg
        $sw.WriteLine($line)

        $written += [System.Text.Encoding]::UTF8.GetByteCount($line) + 2  # +2 for CRLF
        $lineCount++

        if ($lineCount % 100000 -eq 0) {
            $pct = [int]($written / $TargetBytes * 100)
            Write-Host "  $pct% ($lineCount lines)..."
        }
    }
} finally {
    $sw.Close()
}

$actual = (Get-Item $OutputFile).Length
Write-Host "Done! Lines: $lineCount / Size: $([math]::Round($actual/1MB, 2)) MB"