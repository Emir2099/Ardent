# Ardent 3.2 Benchmark Suite
# Compares Interpreter vs VM execution performance

param(
    [string]$ArdentExe = ".\build\ardent.exe",
    [int]$Runs = 3
)

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "===============================================================" -ForegroundColor Cyan
Write-Host "           Ardent 3.2 Performance Benchmark Suite              " -ForegroundColor Cyan
Write-Host "===============================================================" -ForegroundColor Cyan
Write-Host ""

if (-not (Test-Path $ArdentExe)) {
    Write-Host "Error: Ardent executable not found at '$ArdentExe'" -ForegroundColor Red
    Write-Host "Please build the project first: cmake --build build" -ForegroundColor Yellow
    exit 1
}

$benchmarks = @(
    @{ Name = "Arithmetic"; File = "benchmarks\arithmetic_bench.ardent" },
    @{ Name = "Iteration"; File = "benchmarks\iteration_bench.ardent" },
    @{ Name = "Collections"; File = "benchmarks\collections_bench.ardent" },
    @{ Name = "Spells"; File = "benchmarks\spells_bench.ardent" }
)

function Run-Benchmark {
    param(
        [string]$Name,
        [string]$File,
        [string]$Mode
    )
    
    $times = @()
    for ($i = 1; $i -le $Runs; $i++) {
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        
        if ($Mode -eq "interpreter") {
            & $ArdentExe --interpret $File 2>&1 | Out-Null
        } elseif ($Mode -eq "vm") {
            & $ArdentExe --vm $File 2>&1 | Out-Null
        }
        
        $sw.Stop()
        $times += $sw.ElapsedMilliseconds
    }
    
    $avg = ($times | Measure-Object -Average).Average
    return [math]::Round($avg, 2)
}

Write-Host "Running $Runs iterations per benchmark..." -ForegroundColor Gray
Write-Host ""

$results = @()

foreach ($bench in $benchmarks) {
    if (-not (Test-Path $bench.File)) {
        Write-Host "  [SKIP] $($bench.Name) - file not found" -ForegroundColor Yellow
        continue
    }
    
    Write-Host "  [....] $($bench.Name)..." -ForegroundColor Gray -NoNewline
    
    $interpTime = Run-Benchmark -Name $bench.Name -File $bench.File -Mode "interpreter"
    $vmTime = Run-Benchmark -Name $bench.Name -File $bench.File -Mode "vm"
    
    $speedup = if ($vmTime -gt 0) { [math]::Round($interpTime / $vmTime, 2) } else { "N/A" }
    
    $results += @{
        Name = $bench.Name
        Interpreter = $interpTime
        VM = $vmTime
        Speedup = $speedup
    }
    
    Write-Host " done" -ForegroundColor Green
}

Write-Host ""
Write-Host "===============================================================" -ForegroundColor Cyan
Write-Host "                        RESULTS                                " -ForegroundColor Cyan
Write-Host "===============================================================" -ForegroundColor Cyan
Write-Host ""

# Table header
$header = "{0,-15} {1,15} {2,15} {3,12}" -f "Benchmark", "Interpreter(ms)", "VM(ms)", "Speedup"
Write-Host $header -ForegroundColor White
Write-Host ("-" * 60) -ForegroundColor Gray

foreach ($r in $results) {
    $speedupStr = if ($r.Speedup -eq "N/A") { "N/A" } else { "$($r.Speedup)x" }
    $color = if ($r.Speedup -gt 1) { "Green" } elseif ($r.Speedup -lt 1) { "Red" } else { "White" }
    
    $row = "{0,-15} {1,15} {2,15} {3,12}" -f $r.Name, $r.Interpreter, $r.VM, $speedupStr
    Write-Host $row -ForegroundColor $color
}

Write-Host ""
Write-Host "===============================================================" -ForegroundColor Cyan

# Summary
$avgSpeedup = ($results | Where-Object { $_.Speedup -ne "N/A" } | ForEach-Object { $_.Speedup } | Measure-Object -Average).Average
if ($avgSpeedup) {
    $avgSpeedup = [math]::Round($avgSpeedup, 2)
    Write-Host ""
    if ($avgSpeedup -gt 1) {
        Write-Host "  [FAST] Average VM speedup: ${avgSpeedup}x faster than interpreter" -ForegroundColor Green
    } elseif ($avgSpeedup -lt 1) {
        $slowdown = [math]::Round(1 / $avgSpeedup, 2)
        Write-Host "  [SLOW] VM is ${slowdown}x slower (may need optimization)" -ForegroundColor Yellow
    } else {
        Write-Host "  Performance is roughly equivalent" -ForegroundColor White
    }
}

Write-Host ""
Write-Host "Benchmark complete." -ForegroundColor Gray
