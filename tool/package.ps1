param(
    [ValidateSet("zip", "dat")]
    [string]$ArchiveFormat = "zip"
)

$ProjectRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::Join($PSScriptRoot, ".."))
$ReleasesRoot = [System.IO.Path]::Join($ProjectRoot, "build", "releases")
$BuildRootX64 = [System.IO.Path]::Join($ProjectRoot, "build", "windows-x64")
$BinaryRootX64 = [System.IO.Path]::Join($BuildRootX64, "bin")
$ExampleRoot = [System.IO.Path]::Join($ProjectRoot, "data", "example")
$PackageName = "LuaSTG-Retro"
$ExecutableName = "LuastgRetro.exe"

Write-Output "Project Root       : $ProjectRoot"
Write-Output "Releases Root      : $ReleasesRoot"
Write-Output "Binary Root (x64)  : $BinaryRootX64"
Write-Output "Example Root       : $ExampleRoot"
Write-Output "Package Name       : $PackageName"
Write-Output "Executable Name    : $ExecutableName"
Write-Output "Archive Format     : $ArchiveFormat"

# build

Set-Location $ProjectRoot

cmake --workflow --preset windows-x64-release
if ($LASTEXITCODE -ne 0) {
    throw "Failed to build the Windows x64 release"
}

if ($ArchiveFormat -eq "dat") {
    cmake --build $BuildRootX64 --config Release --target dat-archive-builder
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to build dat-archive-builder"
    }
}

# read version info

$ConfigFilePath = [System.IO.Path]::Join($ProjectRoot, "LuaSTG", "LuaSTG", "LConfig.h")
$ConfigFile = [System.IO.File]::ReadAllText($ConfigFilePath, [System.Text.Encoding]::UTF8)
$VersionMajor = "0"
$VersionMinor = "0"
$VersionPatch = "1"
foreach ($Line in $ConfigFile.Split("`n")) {
    if ($Line.Contains("LUASTG_VERSION_MAJOR")) {
        $VersionMajor = $Line.Replace("#define", "").Replace("LUASTG_VERSION_MAJOR", "").Trim()
    }
    if ($Line.Contains("LUASTG_VERSION_MINOR")) {
        $VersionMinor = $Line.Replace("#define", "").Replace("LUASTG_VERSION_MINOR", "").Trim()
    }
    if ($Line.Contains("LUASTG_VERSION_PATCH")) {
        $VersionPatch = $Line.Replace("#define", "").Replace("LUASTG_VERSION_PATCH", "").Trim()
    }
}
$VersionFull = "$VersionMajor.$VersionMinor.$VersionPatch"
$ReleaseRoot = [System.IO.Path]::Join($ReleasesRoot, "$PackageName-v$VersionFull")

Write-Output "Version            : $VersionFull"
Write-Output "Release Root       : $ReleaseRoot"

if (-not [System.IO.Directory]::Exists($ReleaseRoot)) {
    [System.IO.Directory]::CreateDirectory($ReleaseRoot)
}

# copy engine binary file

$BinaryFilesX64 = @(
    @{
        Source = [System.IO.Path]::Join($BinaryRootX64, $ExecutableName)
        Destination = [System.IO.Path]::Join($ReleaseRoot, $ExecutableName)
    },
    @{
        Source = [System.IO.Path]::Join($BinaryRootX64, "d3dcompiler_47.dll")
        Destination = [System.IO.Path]::Join($ReleaseRoot, "d3dcompiler_47.dll")
    }
)

foreach ($BinaryFile in $BinaryFilesX64) {
    if (Test-Path -Path $BinaryFile.Destination) {
        Remove-Item -Path $BinaryFile.Destination
    }
    Copy-Item -Path $BinaryFile.Source -Destination $BinaryFile.Destination
}

# copy example file

$ExampleAssets = [System.IO.Path]::Join($ExampleRoot, "assets")
$ReleaseAssets = [System.IO.Path]::Join($ReleaseRoot, "assets")
$ExampleScripts = [System.IO.Path]::Join($ExampleRoot, "scripts")
$ReleaseScripts = [System.IO.Path]::Join($ReleaseRoot, "scripts")
$DocRoot = [System.IO.Path]::Join($ProjectRoot, "LuaSTG", "doc")
$ReleaseDocRoot = [System.IO.Path]::Join($ReleaseRoot, "doc")
$LicenseRoot = [System.IO.Path]::Join($ProjectRoot, "data", "license")
$ReleaseLicenseRoot = [System.IO.Path]::Join($ReleaseRoot, "license")

if (Test-Path -Path $ReleaseAssets) {
    Remove-Item -Path $ReleaseAssets -Recurse
}
if (Test-Path -Path $ReleaseScripts) {
    Remove-Item -Path $ReleaseScripts -Recurse
}
if (Test-Path -Path $ReleaseDocRoot) {
    Remove-Item -Path $ReleaseDocRoot -Recurse
}
if (Test-Path -Path $ReleaseLicenseRoot) {
    Remove-Item -Path $ReleaseLicenseRoot -Recurse
}
Copy-Item -Path $ExampleAssets -Destination $ReleaseAssets -Recurse
Copy-Item -Path $ExampleScripts -Destination $ReleaseScripts -Recurse
Copy-Item -Path $DocRoot -Destination $ReleaseDocRoot -Recurse -Exclude ".git"
Copy-Item -Path $LicenseRoot -Destination $ReleaseLicenseRoot -Recurse
[System.IO.File]::Copy([System.IO.Path]::Join($ExampleRoot, "config.json"), [System.IO.Path]::Join($ReleaseRoot, "config.json"), $true)
$ReadmePath = [System.IO.Path]::Join($ExampleRoot, "使用说明.txt")
if ([System.IO.File]::Exists($ReadmePath)) {
    [System.IO.File]::Copy($ReadmePath, [System.IO.Path]::Join($ReleaseRoot, "使用说明.txt"), $true)
}

# archive

switch ($ArchiveFormat) {
    "zip" {
        $ArchivePath = [System.IO.Path]::Join($ReleasesRoot, "$PackageName-v$VersionFull.zip")
        Compress-Archive -Path $ReleaseRoot -DestinationPath $ArchivePath -CompressionLevel Optimal -Force
    }
    "dat" {
        $ArchivePath = [System.IO.Path]::Join($ReleasesRoot, "$PackageName-v$VersionFull.dat")
        $DatArchiveBuilder = [System.IO.Path]::Join($BuildRootX64, "tool", "dat-archive-builder", "Release", "dat-archive-builder.exe")
        if (-not [System.IO.File]::Exists($DatArchiveBuilder)) {
            throw "Cannot find dat-archive-builder.exe: $DatArchiveBuilder"
        }
        & $DatArchiveBuilder --input $ReleaseRoot --output $ArchivePath
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to create DAT archive"
        }
    }
}
