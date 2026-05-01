param(
    [string]$NanopbPath = "core/third_party/nanopb",
    [string]$ProtoFile = "proto/migration_payload.proto",
    [string]$OutputDir = "core/generated"
)

$ErrorActionPreference = "Stop"

Write-Host "Generating nanopb protobuf sources..." -ForegroundColor Cyan

# Check if nanopb generator exists
$generator = "$NanopbPath/generator/nanopb_generator.py"
if (-not (Test-Path $generator)) {
    Write-Host "nanopb generator not found at: $generator" -ForegroundColor Red
    Write-Host "Clone nanopb: git submodule add https://github.com/nanopb/nanopb.git $NanopbPath" -ForegroundColor Yellow
    exit 1
}

# Create output directory
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

# Run generator
python $generator $ProtoFile -D $OutputDir

if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ Generated:" -ForegroundColor Green
    Write-Host "  - $OutputDir/migration_payload.pb.c" -ForegroundColor Gray
    Write-Host "  - $OutputDir/migration_payload.pb.h" -ForegroundColor Gray
} else {
    Write-Host "✗ Generation failed" -ForegroundColor Red
    exit 1
}
