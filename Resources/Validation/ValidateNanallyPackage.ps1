[CmdletBinding()]
param(
    [string]$BaselineFile = (Join-Path $PSScriptRoot 'NanallyPackageBaseline.json'),
    [string]$JobFile = 'F:\NTE\PhyLab\Saved\NTEBuildTool\Packages\zzzzzzz_nanally_merged_postbp_0731_P\zzzzzzz_nanally_merged_postbp_0731_P.job.json',
    [string]$PackageDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw "Nanally merged-mesh validation failed: $Message"
    }
}

function Assert-Equal {
    param($Actual, $Expected, [string]$Message)
    if ($Actual -ne $Expected) {
        throw "Nanally merged-mesh validation failed: $Message. Expected '$Expected', got '$Actual'."
    }
}

function Read-Json {
    param([string]$Path)
    Assert-True (Test-Path -LiteralPath $Path -PathType Leaf) "missing JSON file: $Path"
    return Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json
}

$baseline = Read-Json $BaselineFile
$job = Read-Json $JobFile
if ([string]::IsNullOrWhiteSpace($PackageDirectory)) {
    $PackageDirectory = $baseline.DeploymentDirectory
}

Assert-Equal $job.ModName $baseline.ModName 'mod name'
Assert-True (-not [bool]$job.Unversioned) 'cook must use versioned packages'
Assert-Equal @($job.Packages).Count ([int]$baseline.PackageCount) 'explicit package count'

foreach ($requiredPackage in $baseline.RequiredPackages) {
    Assert-True (@($job.Packages) -contains $requiredPackage) "missing required package: $requiredPackage"
}
foreach ($excludedPackage in @($baseline.ExternalPackages) + @($baseline.RemovedPackages)) {
    Assert-True (-not (@($job.Packages) -contains $excludedPackage)) "excluded package was included: $excludedPackage"
}

foreach ($extension in 'pak', 'ucas', 'utoc') {
    $name = "$($baseline.ModName).$extension"
    $staged = Join-Path $job.ModsDir $name
    $deployed = Join-Path $PackageDirectory $name
    Assert-True (Test-Path -LiteralPath $staged -PathType Leaf) "missing staged $extension"
    Assert-True (Test-Path -LiteralPath $deployed -PathType Leaf) "missing deployed $extension"
    Assert-Equal (Get-FileHash -Algorithm SHA256 -LiteralPath $deployed).Hash (Get-FileHash -Algorithm SHA256 -LiteralPath $staged).Hash "$extension deployment hash"
}

[pscustomobject]@{
    Passed = $true
    PackageCount = @($job.Packages).Count
    ModName = $job.ModName
    DeploymentDirectory = $PackageDirectory
}
