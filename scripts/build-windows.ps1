# PowerShell script for building op-sqlite on Windows
param(
    [switch]$Release = $false,
    [switch]$Clean = $false,
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

Write-Host "Building op-sqlite for Windows..." -ForegroundColor Green
Write-Host "Platform: $Platform" -ForegroundColor Cyan
Write-Host "Configuration: $(if($Release) {'Release'} else {'Debug'})" -ForegroundColor Cyan

# Navigate to windows directory
Push-Location $PSScriptRoot\..\windows

try {
    # Clean if requested
    if ($Clean) {
        Write-Host "Cleaning previous builds..." -ForegroundColor Yellow
        msbuild /t:Clean /p:Configuration=Debug /p:Platform=$Platform OPSQLite.sln
        msbuild /t:Clean /p:Configuration=Release /p:Platform=$Platform OPSQLite.sln
    }

    # Determine configuration
    $Configuration = if ($Release) { "Release" } else { "Debug" }

    # Build the solution
    Write-Host "Building $Configuration configuration..." -ForegroundColor Yellow
    $buildResult = msbuild /p:Configuration=$Configuration /p:Platform=$Platform OPSQLite.sln

    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }

    Write-Host "Windows build completed successfully!" -ForegroundColor Green
    
    # Output the built library location
    $outputPath = "OPSQLite\$Platform\$Configuration\OPSQLite.dll"
    if (Test-Path $outputPath) {
        Write-Host "Built library at: $outputPath" -ForegroundColor Cyan
    }
}
finally {
    Pop-Location
}

# Optionally run the example app
if ($args -contains "--run-example") {
    Write-Host "Running example app on Windows..." -ForegroundColor Yellow
    Push-Location $PSScriptRoot\..\example
    npx react-native run-windows
    Pop-Location
}
