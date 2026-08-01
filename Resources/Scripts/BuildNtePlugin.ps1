[CmdletBinding()]
param(
    [string]$EngineRoot = "F:\ue5.6.1\UE_5.6",
    [string]$PackageDirectory,
    [switch]$StrictIncludes
)

$ErrorActionPreference = "Stop"

$pluginRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$pluginFile = Join-Path $pluginRoot "NTEBuildTool.uplugin"
$uat = Join-Path $EngineRoot "Engine\Build\BatchFiles\RunUAT.bat"

if ([string]::IsNullOrWhiteSpace($PackageDirectory)) {
    $buildRoot = Join-Path (Split-Path $pluginRoot -Parent) ".plugin-builds"
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $PackageDirectory = Join-Path $buildRoot "NTEBuildTool_$timestamp"
}

$packageFullPath = [System.IO.Path]::GetFullPath($PackageDirectory)
$pluginPrefix = $pluginRoot.TrimEnd("\") + "\"
if ($packageFullPath.Equals($pluginRoot, [System.StringComparison]::OrdinalIgnoreCase) -or
    $packageFullPath.StartsWith($pluginPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "BuildPlugin output must be outside the plugin root to prevent recursive HostProject copies: $packageFullPath"
}

if (-not (Test-Path -LiteralPath $pluginFile)) {
    throw "Plugin descriptor not found: $pluginFile"
}
if (-not (Test-Path -LiteralPath $uat)) {
    throw "RunUAT not found: $uat"
}
if (Test-Path -LiteralPath $packageFullPath) {
    throw "Package output already exists; choose a new directory: $packageFullPath"
}

$arguments = @(
    "BuildPlugin",
    "-Plugin=$pluginFile",
    "-Package=$packageFullPath",
    "-TargetPlatforms=Win64"
)
if ($StrictIncludes) {
    $arguments += "-StrictIncludes"
}

Write-Output "Plugin root: $pluginRoot"
Write-Output "Package output: $packageFullPath"
& $uat @arguments
if ($LASTEXITCODE -ne 0) {
    throw "BuildPlugin failed with exit code $LASTEXITCODE"
}

Write-Output "BuildPlugin completed: $packageFullPath"
