$ErrorActionPreference = 'Stop'
# Launches the already-built Qt6 verification GUI (apps/verification-gui). The GUI itself starts
# the backend (kopruq_solver_cli.exe, built into the same build/gui-release/ directory) via
# QProcess when the user clicks "Modeli Doğrula"/"Solver'ı Çalıştır" -- there is no separate
# backend process to start by hand.
#
# Build it first if you haven't:
#   .\scripts\configure.ps1 Gui
#   .\scripts\build.ps1 Gui
# The first configure fetches and builds Qt6/xlnt from source via vcpkg, which can take well
# over an hour with no local cache; subsequent configure/build calls are fast.
$exe = Join-Path $PSScriptRoot '..\build\gui-release\kopruq_verification_gui.exe'
if (-not (Test-Path $exe)) {
  throw "GUI executable not found: $exe`nBuild it first: .\scripts\configure.ps1 Gui && .\scripts\build.ps1 Gui"
}
$cli = Join-Path $PSScriptRoot '..\build\gui-release\kopruq_solver_cli.exe'
if (-not (Test-Path $cli)) {
  Write-Warning "kopruq_solver_cli.exe not found next to the GUI exe -- 'Solver'i Çalıştır' will fail to find it."
}
Start-Process -FilePath $exe
