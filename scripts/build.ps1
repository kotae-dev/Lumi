param(
    [string]$Config = "Release",
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

Write-Host "╔══════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║         Lumi Build System            ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════╝" -ForegroundColor Cyan

# Check prerequisites
Write-Host "`n[1/4] Checking prerequisites..." -ForegroundColor Yellow

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    Write-Host "  ✗ CMake not found. Install from https://cmake.org/" -ForegroundColor Red
    exit 1
}
Write-Host "  ✓ CMake: $($cmake.Version)" -ForegroundColor Green

$dotnet = Get-Command dotnet -ErrorAction SilentlyContinue
if (-not $dotnet) {
    Write-Host "  ✗ .NET SDK not found. Install from https://dot.net" -ForegroundColor Red
    exit 1
}
Write-Host "  ✓ dotnet: $(dotnet --version)" -ForegroundColor Green

# Check for Visual Studio
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWhere) {
    $vsPath = & $vsWhere -latest -property installationPath
    Write-Host "  ✓ Visual Studio: $vsPath" -ForegroundColor Green
}

# Configure CMake
Write-Host "`n[2/4] Configuring CMake..." -ForegroundColor Yellow
cmake -B $BuildDir -G "Visual Studio 18 2026" -A x64 -DCMAKE_BUILD_TYPE=$Config

if ($LASTEXITCODE -ne 0) {
    Write-Host "  ✗ CMake configuration failed" -ForegroundColor Red
    exit 1
}
Write-Host "  ✓ CMake configured" -ForegroundColor Green

# Build C++ core
Write-Host "`n[3/4] Building C++ core (lumi_core.dll)..." -ForegroundColor Yellow
cmake --build $BuildDir --config $Config --target lumi_core --parallel

if ($LASTEXITCODE -ne 0) {
    Write-Host "  ✗ C++ build failed" -ForegroundColor Red
    exit 1
}
Write-Host "  ✓ lumi_core.dll built" -ForegroundColor Green

# Build C# UI
Write-Host "`n[4/4] Building WPF UI (Lumi.exe)..." -ForegroundColor Yellow
dotnet publish ui/Lumi.csproj -c $Config -r win-x64 --self-contained false -o "$BuildDir/bin/"

if ($LASTEXITCODE -ne 0) {
    Write-Host "  ✗ WPF build failed" -ForegroundColor Red
    exit 1
}

# Ensure lumi_core.dll is alongside Lumi.exe
$dllPath = "$BuildDir/bin/$Config/lumi_core.dll"
if (Test-Path $dllPath) {
    Copy-Item $dllPath -Destination "$BuildDir/bin/" -Force
}
Write-Host "  ✓ Lumi.exe built" -ForegroundColor Green

Write-Host "`n═══════════════════════════════════════" -ForegroundColor Cyan
Write-Host "✅ Build complete! Output: $BuildDir\bin\" -ForegroundColor Green
Write-Host "═══════════════════════════════════════" -ForegroundColor Cyan
