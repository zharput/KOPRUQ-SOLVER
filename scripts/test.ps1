$ErrorActionPreference = 'Stop'
$configuration = if ($args.Count -gt 0) { $args[0] } else { 'Debug' }
$vsRoot = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
$env:VCPKG_ROOT = 'C:\_ZHarput_Data\Tools\vcpkg'
$ctest = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
if ($configuration -notin @('Debug','Release')) { throw 'Configuration must be Debug or Release' }
$dir = if ($configuration -eq 'Debug') { 'debug' } else { 'release' }
cmd.exe /d /s /c ('call "' + $vsRoot + '\VC\Auxiliary\Build\vcvars64.bat" >nul && set "VCPKG_ROOT=' + $env:VCPKG_ROOT + '" && "' + $ctest + '" --test-dir build/' + $dir + ' --output-on-failure')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
