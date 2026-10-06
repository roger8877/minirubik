param([string]$Src, [string]$Proc = "RV32_ISS")
$ripes = "C:\Users\N96141529\Desktop\computer_architecture\Ripes-v2.2.6-106-g5b8a616-win-x86_64\Ripes.exe"
$out = "$Src.$Proc.txt"
$argList = "--mode cli -t asm --src $Src --proc $Proc --iret --exectime --output $out"
$sw = [Diagnostics.Stopwatch]::StartNew()
$p = Start-Process $ripes -ArgumentList $argList -PassThru
$peakWS = 0; $peakPriv = 0
while (-not $p.HasExited) {
    try {
        $p.Refresh()
        $peakWS   = [Math]::Max($peakWS,   $p.PeakWorkingSet64)
        $peakPriv = [Math]::Max($peakPriv, $p.PrivateMemorySize64)
    } catch {}
    Start-Sleep -Milliseconds 50
}
$sw.Stop()
$report = Get-Content $out
$iret = $report[[Array]::IndexOf($report, "===== instructions retired") + 1]
$exec = $report[[Array]::IndexOf($report, "===== wall-clock model execution time (ms)") + 1]
"{0} [{1}]  iret={2}  exectime={3} ms  peakWS={4:N1} MiB  peakPrivate={5:N1} MiB  wall={6:N2} s" -f `
    $Src, $Proc, $iret, $exec, ($peakWS/1MB), ($peakPriv/1MB), $sw.Elapsed.TotalSeconds

# Append every run so earlier results are never overwritten.
$log = "results_log.csv"
if (-not (Test-Path $log)) {
    "timestamp,program,proc,iret,exectime_ms,wall_s,peak_ws_mib,peak_private_mib" | Out-File $log -Encoding utf8
}
"{0},{1},{2},{3},{4},{5:F2},{6:F1},{7:F1}" -f (Get-Date -Format s), $Src, $Proc, $iret, $exec, `
    $sw.Elapsed.TotalSeconds, ($peakWS/1MB), ($peakPriv/1MB) | Out-File $log -Append -Encoding utf8
