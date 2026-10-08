# Runs every host test suite in test/ with .devtools\ntest.ps1, compiling each test together
# with the src/ modules it (transitively) #includes.
#   powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
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
$fail = 0; $pass = 0
foreach ($t in Get-ChildItem test -Directory) {
    $tf = Get-ChildItem $t.FullName -Filter *.cpp | Select-Object -First 1
    $seen = New-Object 'System.Collections.Generic.HashSet[string]'
    Deps $tf.FullName $seen
    $mods = @($seen | Where-Object { $_ -like "*.cpp" })
    $o = & powershell -ExecutionPolicy Bypass -File .devtools\ntest.ps1 $tf.FullName @mods 2>&1
    if ($LASTEXITCODE -eq 0) { $pass++; Write-Host "PASS $($t.Name): $(($o | Select-String 'Tests').Line)" }
    else { $fail++; Write-Host "FAIL $($t.Name)"; $o | Select-Object -Last 15 }
}
Write-Host "suites pass=$pass fail=$fail"
exit $fail
