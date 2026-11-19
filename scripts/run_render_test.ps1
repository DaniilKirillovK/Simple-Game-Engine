<#
Автоматизированный оффскрин-рендер-тест: одна команда.
Собирает проект (если нужно), прогоняет фиксированную сцену без окна на D3D12 и
Vulkan, сверяет кадры с PNG-эталонами (порог допуска: max diff <= 2/255,
<= 0.1% отличающихся пикселей, MAE <= 0.2) и завершается с ненулевым кодом
при любом расхождении.

Использование:
  .\scripts\run_render_test.ps1                     # сборка + D3D12 + Vulkan + сверка
  .\scripts\run_render_test.ps1 -Api d3d12          # только D3D12
  .\scripts\run_render_test.ps1 -SkipBuild          # без пересборки
  .\scripts\run_render_test.ps1 -UpdateReference    # пересоздать эталоны

Эталоны: tests/render/golden/<d3d12|vulkan>/scene.png
#>
param(
    [ValidateSet("d3d12", "vulkan", "all")]
    [string]$Api = "all",
    [switch]$SkipBuild,
    [switch]$UpdateReference
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "out\build\x64-debug"
$engine = Join-Path $buildDir "GameEngine.exe"
$goldenDir = Join-Path $root "tests\render\golden"

if (-not (Test-Path $engine)) {
    Write-Error "GameEngine.exe не найден: $engine (собери проект или запусти без -SkipBuild)"
    exit 1
}

if (-not $SkipBuild) {
    Write-Host "== Сборка ==" -ForegroundColor Cyan
    $vcvars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    $cmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    Get-Process GameEngine -ErrorAction SilentlyContinue | Stop-Process -Force
    cmd /c "`"$vcvars`" >nul 2>&1 && `"$cmake`" --build `"$buildDir`" --config Debug --parallel"
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Сборка упала (код $LASTEXITCODE)"
        exit $LASTEXITCODE
    }
}

$testArgs = @("--render-test", $Api, "--golden", $goldenDir)
if ($UpdateReference) { $testArgs += "--update-reference" }

Write-Host "== Прогон: GameEngine $($testArgs -join ' ') ==" -ForegroundColor Cyan
Push-Location $buildDir
try {
    & $engine @testArgs
    $code = $LASTEXITCODE
}
finally {
    Pop-Location
}

if ($code -ne 0) {
    Write-Host "Render test FAILED (exit $code). Лог: $buildDir\engine.log" -ForegroundColor Red
}
else {
    Write-Host "Render test PASSED" -ForegroundColor Green
}
exit $code
