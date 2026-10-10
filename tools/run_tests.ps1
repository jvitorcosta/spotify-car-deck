# Runs every host test suite in test/ with tools/ntest.ps1, compiling each test together
# with the src/ modules it (transitively) #includes. Windows PowerShell or PowerShell 7 (CI).
#   powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1     (Windows)
#   pwsh tools/run_tests.ps1                                          (Linux/macOS)
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
$ps = (Get-Process -Id $PID).Path                  # run ntest with this same PowerShell
$ntest = Join-Path $PSScriptRoot "ntest.ps1"
function Deps($file, $seen) {
    $dir = Split-Path $file -Parent
    foreach ($m in (Select-String -Path $file -Pattern '#include\s+"([^"]+)"' -AllMatches).Matches) {
        $inc = [System.IO.Path]::GetFullPath((Join-Path $dir $m.Groups[1].Value))
        if (-not (Test-Path $inc) -or $seen.Contains($inc)) { continue }
        [void]$seen.Add($inc)
        Deps $inc $seen
        if ($inc -like "*.h") {
            $cpp = [System.IO.Path]::ChangeExtension($inc, ".cpp")
            if ((Test-Path $cpp) -and -not $seen.Contains($cpp)) { [void]$seen.Add($cpp); Deps $cpp $seen }
        }
    }
}
$fail = 0; $pass = 0; $failed = @()
foreach ($t in Get-ChildItem test -Directory) {
    $tf = Get-ChildItem $t.FullName -Filter *.cpp | Select-Object -First 1
    if (-not $tf) { $fail++; $failed += $t.Name; Write-Host "FAIL $($t.Name): no .cpp in the folder"; continue }
    $seen = New-Object 'System.Collections.Generic.HashSet[string]'
    Deps $tf.FullName $seen
    $mods = @($seen | Where-Object { $_ -like "*.cpp" })
    $o = & $ps -NoProfile -ExecutionPolicy Bypass -File $ntest $tf.FullName @mods 2>&1
    $summary = ($o | Select-String '^\d+ Tests').Line
    if ($LASTEXITCODE -eq 0 -and $summary -and $summary -notmatch '^0 Tests') {
        $pass++; Write-Host "PASS $($t.Name): $summary"
    } else {
        # The whole output: the first compiler error or the sanitizer report's header is the useful part.
        $fail++; $failed += $t.Name; Write-Host "FAIL $($t.Name)"; $o | ForEach-Object { Write-Host "  $_" }
        if ($env:GITHUB_ACTIONS) {   # Unity's file:line:test:FAIL: msg -> inline annotations on the PR
            foreach ($m in ($o | Select-String '^(.+?):(\d+):(\w+):FAIL:?\s*(.*)$').Matches) {
                Write-Host "::error file=$($m.Groups[1].Value),line=$($m.Groups[2].Value)::$($m.Groups[3].Value): $($m.Groups[4].Value)"
            }
        }
    }
}
Write-Host "suites pass=$pass fail=$fail$(if ($failed) { ' (' + ($failed -join ', ') + ')' })"
exit $fail
