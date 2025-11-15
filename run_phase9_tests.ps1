$tests = @("numbers_test","phrases_test","arithmetic_test","variables_test","let_assign_test","if_else_test","while_test","spell_declare_test","spell_return_test","combination_test")
$outdir = Join-Path $PSScriptRoot "build\phase9_out"
if (-not (Test-Path $outdir)) { New-Item -ItemType Directory -Path $outdir -Force | Out-Null }
foreach ($t in $tests) {
    $in = Join-Path $PSScriptRoot "test_scrolls\$t.ardent"
    $interpOut = Join-Path $outdir ($t + ".interp.txt")
    $jitOut = Join-Path $outdir ($t + ".jit.txt")
    Write-Host "Running test: $t"
    & .\build\ardent.exe --interpret --quiet-assign $in > $interpOut
    & .\build\ardent.exe --llvm $in > $jitOut
    if ($LASTEXITCODE -ne 0) { Write-Host "JIT exit code: $LASTEXITCODE" }
    Write-Host "Comparing outputs for $t"
    $a = Get-Content $interpOut | Where-Object { $_ -notmatch '^(\[LLVM JIT\])' }
    $b = Get-Content $jitOut | Where-Object { $_ -notmatch '^(\[LLVM JIT\])' }
    if (($a -join "`n") -eq ($b -join "`n")) {
        Write-Host "OK: $t outputs match"
    } else {
        Write-Host "MISMATCH: $t"
        Write-Host "-- Interpreter --"
        $a
        Write-Host "-- JIT --"
        $b
    }
    Write-Host "----------------------------------------"
}
Write-Host "All tests completed. Output files in: $outdir"