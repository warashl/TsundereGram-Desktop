# Builds the Telegram target and keeps a log with the compiler errors.
param([string]$Label = 'build')
$ErrorActionPreference = 'Continue'
$out = Join-Path $env:TBUILD "$env:REPO_NAME\out"
$log = Join-Path $env:GITHUB_WORKSPACE "$Label.log"
& cmake --build $out --config Release --target Telegram --parallel 2 2>&1 | Tee-Object -FilePath $log
$code = $LASTEXITCODE
if ($code -ne 0) {
  $errors = Select-String -Path $log -Pattern ': (fatal )?error |error LNK|error MSB|CMake Error' |
    Select-Object -First 200 | ForEach-Object { $_.Line }
  $tail = Get-Content $log -Tail 80
  $report = @("# $Label failed (exit $code)", '', '## errors', '') + $errors + @('', '## tail', '') + $tail
  $file = Join-Path $env:GITHUB_WORKSPACE "$Label-errors.txt"
  $report | Set-Content -Path $file -Encoding utf8
  Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value ('```' + "`n" + (($errors | Select-Object -First 60) -join "`n") + "`n" + '```')
  exit 1
}
exit 0
