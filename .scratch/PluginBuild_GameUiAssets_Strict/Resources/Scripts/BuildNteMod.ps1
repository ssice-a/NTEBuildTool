param(
    [Parameter(Mandatory = $true)]
    [string]$JobFile,
    [switch]$SkipCook
)

$ErrorActionPreference = "Stop"

function Assert-PathExists {
    param([string]$Path, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Label not found: $Path"
    }
}

function Quote-PathForResponse {
    param([string]$Path)
    return '"' + $Path + '"'
}

function Convert-PackageToContentPath {
    param([string]$PackageName)
    if (-not $PackageName.StartsWith("/Game/")) {
        throw "Only /Game packages are supported: $PackageName"
    }

    return ($PackageName.Substring("/Game/".Length) -replace "/", "\")
}

function Convert-ToCookedRelativePath {
    param([string]$ProjectName, [string]$ProjectRelativeContentPath)
    return Join-Path "$ProjectName\Content" $ProjectRelativeContentPath
}

function Convert-ToGameMountPath {
    param([string]$GameMountName, [string]$ProjectRelativeContentPath)
    return "../../../$GameMountName/Content/" + ($ProjectRelativeContentPath -replace "\\", "/")
}

function Test-IsGeneratedRuntimeBlueprintPackage {
    param([string]$PackageName)

    $assetName = ($PackageName -split "/")[-1]
    if ($assetName.StartsWith("ABP_NTE_ModToggle_") -or
        $assetName.StartsWith("WBP_NTE_ModToggle") -or
        $assetName.StartsWith("BP_NTE_ModToggle")) {
        return $true
    }

    if ($PackageName -match "(?i)/mod/Runtime/" -and
        ($assetName.StartsWith("ABP_") -or $assetName.StartsWith("WBP_") -or $assetName.StartsWith("BP_"))) {
        return $true
    }

    return $false
}

function Add-CookedFilesForPackage {
    param(
        [System.Collections.Generic.List[string]]$Lines,
        [string]$CookedRoot,
        [string]$ProjectName,
        [string]$GameMountName,
        [string]$PackageName
    )

    $relativeContentPath = Convert-PackageToContentPath $PackageName
    # IoStore resolves export data for package assets from the cooked package store.
    # Adding .uexp as a loose response entry makes UnrealPak warn and can produce
    # containers where the package export serial size no longer matches at load time.
    $extensions = @(".uasset", ".ubulk", ".uptnl")

    foreach ($extension in $extensions) {
        $sourceRelative = Convert-ToCookedRelativePath $ProjectName ($relativeContentPath + $extension)
        $sourcePath = Join-Path $CookedRoot $sourceRelative
        if (Test-Path -LiteralPath $sourcePath) {
            $destPath = Convert-ToGameMountPath $GameMountName ($relativeContentPath + $extension)
            $Lines.Add("$(Quote-PathForResponse $sourcePath) $(Quote-PathForResponse $destPath)")
        }
    }
}

Assert-PathExists $JobFile "Build job"

$job = Get-Content -LiteralPath $JobFile -Raw -Encoding UTF8 | ConvertFrom-Json

$projectFile = [string]$job.ProjectFile
$projectRoot = [string]$job.ProjectRoot
$projectName = [string]$job.ProjectName
$engineRoot = [string]$job.EngineRoot
$gameMountName = [string]$job.GameMountName
$modsDir = [string]$job.ModsDir
$modName = [string]$job.ModName
$mode = [string]$job.Mode
$packages = @($job.Packages)
$neverPackPrefixes = @($job.NeverPackPackagePrefixes)
$jobSkipCook = $false
if ($null -ne $job.SkipCook) {
    $jobSkipCook = [bool]$job.SkipCook
}
if ($SkipCook) {
    $jobSkipCook = $true
}
$useUnversioned = $false
if ($null -ne $job.Unversioned) {
    $useUnversioned = [bool]$job.Unversioned
}
if ([string]::IsNullOrWhiteSpace($mode)) {
    $mode = if ($jobSkipCook) { "PackOnly" } else { "CookAndPack" }
}

if ([string]::IsNullOrWhiteSpace($projectName)) {
    $projectName = [System.IO.Path]::GetFileNameWithoutExtension($projectFile)
}

if ([string]::IsNullOrWhiteSpace($gameMountName)) {
    $gameMountName = $projectName
}

if ([string]::IsNullOrWhiteSpace($modName)) {
    throw "ModName is empty."
}

if ($packages.Count -eq 0) {
    throw "No packages were selected."
}

if ($useUnversioned) {
    foreach ($package in $packages) {
        if (Test-IsGeneratedRuntimeBlueprintPackage $package) {
            Write-Warning "Package '$package' is a generated runtime Blueprint package. Disabling Unversioned cook to avoid Widget Blueprint serialization errors in the target game."
            $useUnversioned = $false
            break
        }
    }
}

$engineRoot = $engineRoot.TrimEnd("\", "/")
$unrealEditorCmd = Join-Path $engineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$unrealPak = Join-Path $engineRoot "Engine\Binaries\Win64\UnrealPak.exe"

Assert-PathExists $projectFile "Project"
Assert-PathExists $projectRoot "Project root"
Assert-PathExists $unrealEditorCmd "UnrealEditor-Cmd"
Assert-PathExists $unrealPak "UnrealPak"

$editorProcess = Get-Process UnrealEditor -ErrorAction SilentlyContinue
if ($editorProcess) {
    Write-Warning "UnrealEditor is running. This job uses saved on-disk assets only."
}

foreach ($package in $packages) {
    foreach ($prefix in $neverPackPrefixes) {
        if (-not [string]::IsNullOrWhiteSpace($prefix) -and $package.StartsWith($prefix)) {
            throw "Refusing to pack package '$package' because it matches forbidden prefix '$prefix'."
        }
    }
}

$workRoot = Join-Path $projectRoot "Saved\NTEBuildTool\Packages\$modName"
$cookOutput = Join-Path $workRoot "Cooked"
$responseFile = Join-Path $workRoot "$modName.response.txt"
$commandsFile = Join-Path $workRoot "$modName.iostore.txt"
$containerBase = Join-Path $workRoot $modName
$emptyPakResponse = Join-Path $workRoot "$modName.empty-pak.txt"
$globalContainerBase = Join-Path $workRoot "global"

New-Item -ItemType Directory -Force -Path $workRoot | Out-Null
New-Item -ItemType Directory -Force -Path $modsDir | Out-Null

if ($mode -ne "PackOnly" -and -not $jobSkipCook) {
    if (Test-Path -LiteralPath $cookOutput) {
        Remove-Item -LiteralPath $cookOutput -Recurse -Force
    }

    $packageArg = "-Package=" + ($packages -join "+")
    $cookArgs = @(
        "`"$projectFile`"",
        "-run=Cook",
        "-TargetPlatform=Windows",
        "-OutputDir=`"$cookOutput`"",
        $packageArg,
        "-NoDefaultMaps",
        "-NoGameAlwaysCook",
        "-SkipSoftReferences",
        "-SkipHardReferences",
        "-SkipEditorContent",
        "-CookSkipRequests",
        "-unattended",
        "-nop4"
    )

    if ($useUnversioned) {
        $cookArgs += "-Unversioned"
    }

    Write-Host "Cooking selected packages..."
    foreach ($package in $packages) {
        Write-Host "  $package"
    }

    & $unrealEditorCmd @cookArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Cook failed with exit code $LASTEXITCODE"
    }
}

if ($mode -eq "CookOnly") {
    Write-Host "CookOnly mode finished."
    exit 0
}

$candidateCookRoots = @(
    (Join-Path $cookOutput "Windows"),
    $cookOutput
)

$cookPlatformRoot = $null
foreach ($candidateCookRoot in $candidateCookRoots) {
    if (Test-Path -LiteralPath (Join-Path $candidateCookRoot "$projectName\Content")) {
        $cookPlatformRoot = $candidateCookRoot
        break
    }
}

if ([string]::IsNullOrWhiteSpace($cookPlatformRoot)) {
    throw "Cooked project content directory not found under: $cookOutput"
}

$responseLines = [System.Collections.Generic.List[string]]::new()
foreach ($package in $packages) {
    Add-CookedFilesForPackage -Lines $responseLines -CookedRoot $cookPlatformRoot -ProjectName $projectName -GameMountName $gameMountName -PackageName $package
}

if ($responseLines.Count -eq 0) {
    throw "No cooked files collected. Check cook output: $cookPlatformRoot"
}

Set-Content -LiteralPath $responseFile -Value $responseLines -Encoding ascii
Set-Content -LiteralPath $commandsFile -Value "Output=`"$containerBase`" ContainerName=$modName ResponseFile=`"$responseFile`"" -Encoding ascii
Set-Content -LiteralPath $emptyPakResponse -Value "" -Encoding ascii

Remove-Item -LiteralPath "$containerBase.pak", "$containerBase.utoc", "$containerBase.ucas", "$globalContainerBase.utoc", "$globalContainerBase.ucas" -Force -ErrorAction SilentlyContinue

Write-Host "Creating empty pak..."
& $unrealPak "$containerBase.pak" "-Create=$emptyPakResponse"
if ($LASTEXITCODE -ne 0) {
    throw "Empty pak creation failed with exit code $LASTEXITCODE"
}

$projectStore = Join-Path $cookPlatformRoot "ue.projectstore"
$packageStoreManifest = Join-Path $cookPlatformRoot "$projectName\Metadata\packagestore.manifest"
$scriptObjects = Join-Path $cookPlatformRoot "$projectName\Metadata\scriptobjects.bin"

$ioStoreArgs = @(
    "`"$projectFile`"",
    "-CreateGlobalContainer=`"$globalContainerBase`"",
    "-CookedDirectory=`"$cookPlatformRoot`"",
    "-Commands=`"$commandsFile`""
)

if (Test-Path -LiteralPath $projectStore) {
    $ioStoreArgs += "-ProjectStore=`"$projectStore`""
} elseif (Test-Path -LiteralPath $packageStoreManifest) {
    $ioStoreArgs += "-PackageStoreManifest=`"$packageStoreManifest`""
} else {
    throw "Neither ue.projectstore nor packagestore.manifest was found under cooked output: $cookPlatformRoot"
}

if (Test-Path -LiteralPath $scriptObjects) {
    $ioStoreArgs += "-ScriptObjects=`"$scriptObjects`""
}

Write-Host "Creating IoStore container..."
& $unrealPak @ioStoreArgs
if ($LASTEXITCODE -ne 0) {
    throw "IoStore creation failed with exit code $LASTEXITCODE"
}

Assert-PathExists "$containerBase.utoc" "Output utoc"
Assert-PathExists "$containerBase.ucas" "Output ucas"

Copy-Item -LiteralPath "$containerBase.pak" -Destination (Join-Path $modsDir "$modName.pak") -Force
Copy-Item -LiteralPath "$containerBase.utoc" -Destination (Join-Path $modsDir "$modName.utoc") -Force
Copy-Item -LiteralPath "$containerBase.ucas" -Destination (Join-Path $modsDir "$modName.ucas") -Force

Write-Host "Done."
Write-Host "Output:"
Write-Host "  $(Join-Path $modsDir "$modName.pak")"
Write-Host "  $(Join-Path $modsDir "$modName.utoc")"
Write-Host "  $(Join-Path $modsDir "$modName.ucas")"
Write-Host ""
Write-Host "Packed files:"
Get-Content -LiteralPath $responseFile | ForEach-Object { Write-Host "  $_" }
