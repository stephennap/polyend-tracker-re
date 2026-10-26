# Builds the OpenTracker simulator on Windows from PowerShell, through MSYS2's
# UCRT64 environment (see sim/README.md, "Windows (MSYS2)").
#   sim\build-windows.ps1            -> build\sim\opentracker-sim.exe
#   sim\build-windows.ps1 check      -> headless checks
#   sim\build-windows.ps1 clean
# Set MSYS2_ROOT if MSYS2 is not installed in C:\msys64.
param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Targets)

$ErrorActionPreference = 'Stop'
$msys = if ($env:MSYS2_ROOT) { $env:MSYS2_ROOT } else { 'C:\msys64' }
$bash = Join-Path $msys 'usr\bin\bash.exe'
if (-not (Test-Path $bash)) {
    Write-Error "MSYS2 not found at $msys (install it with: winget install MSYS2.MSYS2, or set MSYS2_ROOT)"
}

$repo = Split-Path -Parent $PSScriptRoot
$env:MSYSTEM = 'UCRT64'
$env:CHERE_INVOKING = '1'   # start the login shell in the current directory
$jobs = [Environment]::ProcessorCount
$makeArgs = ($Targets | ForEach-Object { "'" + ($_ -replace "'", "'\''") + "'" }) -join ' '

Push-Location $repo
try {
    & $bash -lc "make -C sim -j$jobs $makeArgs"
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
