Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$bashScript = Join-Path $scriptDir "package_windows_release.sh"

if (-not (Test-Path -LiteralPath $bashScript)) {
    throw "Missing packaging script: $bashScript"
}

$gitBashCandidates = @(
    "$env:ProgramFiles\Git\bin\bash.exe",
    "$env:ProgramFiles\Git\usr\bin\bash.exe",
    "${env:ProgramFiles(x86)}\Git\bin\bash.exe",
    "${env:ProgramFiles(x86)}\Git\usr\bin\bash.exe"
) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }

if (-not $gitBashCandidates) {
    throw "Git for Windows bash.exe was not found. Install Git for Windows or run this from Git Bash with: bash ./tools/package_windows_release.sh"
}

$bashPath = $gitBashCandidates[0]
$resolvedScript = (Resolve-Path -LiteralPath $bashScript).Path

if ($resolvedScript -match '^([A-Za-z]):\\(.*)$') {
    $drive = $matches[1].ToLowerInvariant()
    $rest = $matches[2].Replace('\', '/')
    $bashScriptForBash = "/$drive/$rest"
}
else {
    $bashScriptForBash = $resolvedScript.Replace('\', '/')
}

Write-Host "Using Git Bash: $bashPath"
Write-Host "Running: $bashScriptForBash"

& $bashPath $bashScriptForBash
exit $LASTEXITCODE
