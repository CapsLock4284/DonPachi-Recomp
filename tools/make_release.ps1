<#
    Name:        make_release.ps1
    Description: the player's package of the game: release\DonPachi-<version>-win64.zip, for a GitHub Release
    Version:     0.1.0
    Created:     10-11-2026

    license:BSD-3-Clause

    Run it from the project's folder after a Release build:
        cmake --build build/msvc --config Release
        powershell -ExecutionPolicy Bypass -File tools\make_release.ps1

    The version is CMakeLists.txt's (project(donpachi VERSION x.y.z)), the one the game shows on its main menu: change
    it there, build, then package. -Version, if given, must be the same.

    It copies only the files named below (an allow list: nothing else can get in), then checks the folder: no ROM
    set, no extracted game data, no EEPROM, high scores or settings of yours, and an .exe that needs nothing but
    SDL2.dll beside it. Any problem stops it before the .zip is made.

    The package holds the game's program, compiled; the player brings the ROM set (MAME's donpachi.zip, US), which
    the game asks for on its first run (graphics, sound and the program's data come from it).
#>
param(
    [string]$Version = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# the version: CMakeLists.txt's, so the package and the game's main menu agree
$cmake = Get-Content "CMakeLists.txt" -Raw
if ($cmake -notmatch 'project\(donpachi\s+VERSION\s+(\d+\.\d+\.\d+)') { throw "No 'project(donpachi VERSION x.y.z' in CMakeLists.txt" }
$built = $Matches[1]
if ($Version -eq "") { $Version = $built }
elseif ($Version -ne $built) {
    throw "-Version $Version, but CMakeLists.txt says $built (the game would show $built): change it there and build again"
}

if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw "The version is three numbers, as 0.1.0 (not '$Version')" }

$name = "DonPachi-$Version-win64"
$out = Join-Path $root "release\$name"
$zip = Join-Path $root "release\$name.zip"

# ---- what goes in: the only files the package can hold (from -> to) ----
$sdlLicense = Get-ChildItem "build\downloads\sdl2\SDL2-*\LICENSE.txt" -ErrorAction SilentlyContinue | Select-Object -First 1
$files = [ordered]@{
    "build\bin\donpachi.exe"              = "donpachi.exe"
    "build\bin\SDL2.dll"                  = "SDL2.dll"
    "ports\sdl2\release\README.txt"       = "README.txt"
    "ports\sdl2\release\NOTICES.txt"      = "licenses\NOTICES.txt"
    "runtime\LICENSE.tmnt-runtime"        = "licenses\tmntarcade-runtime.txt"
}
if ($sdlLicense) { $files[$sdlLicense.FullName.Substring($root.Length + 1)] = "licenses\SDL2.txt" }
else { throw "No SDL2 LICENSE.txt in build\downloads\sdl2 (the SDL2 development package unpacked there)" }

foreach ($from in $files.Keys) {
    if (-not (Test-Path $from -PathType Leaf)) { throw "Missing: $from (build the Release first?)" }
}

# the .exe newer than the program's sources: a build made after the last change
$exe = Get-Item "build\bin\donpachi.exe"
$newest = Get-ChildItem "ports\sdl2", "runtime" -Recurse -Include *.c, *.h | Sort-Object LastWriteTime | Select-Object -Last 1
if ($newest.LastWriteTime -gt $exe.LastWriteTime) {
    throw "build\bin\donpachi.exe is older than $($newest.FullName): build the Release again first"
}

# ---- the folder, made fresh ----
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
if (Test-Path $zip) { Remove-Item $zip -Force }
New-Item -ItemType Directory -Path (Join-Path $out "licenses") -Force | Out-Null
foreach ($from in $files.Keys) {
    Copy-Item $from (Join-Path $out $files[$from])
}
# the text files with Windows' line ends (for Notepad), the version in the README
foreach ($text in "README.txt", "licenses\NOTICES.txt") {
    $path = Join-Path $out $text
    $content = (Get-Content $path -Raw).Replace("`r`n", "`n").Replace("`n", "`r`n").Replace("{VERSION}", $Version)
    Set-Content $path $content -NoNewline -Encoding ASCII
}

# ---- the checks ----
$problems = @()
$allowed = @($files.Values)
foreach ($file in Get-ChildItem $out -Recurse -File) {
    $relative = $file.FullName.Substring($out.Length + 1)
    if ($allowed -notcontains $relative) { $problems += "not on the list: $relative" }
    if ($file.Extension -in ".zip", ".7z", ".dat", ".part", ".eeprom", ".hiscore", ".achievements", ".ini", ".bin", ".rom", ".xml") {
        $problems += "a ROM set, game data, save or settings file: $relative"
    }
}
# the ROM set's own files (MAME's names), in case one were renamed into the list
foreach ($romName in "prgu.u29", "atdp.u44", "atdp.u45", "atdp.u54", "atdp.u57", "text.u58", "atdp.u32", "atdp.u33",
        "eeprom-donpachi.u10", "donpachi_usa.dat") {
    if (Get-ChildItem $out -Recurse -Filter $romName) { $problems += "a ROM file: $romName" }
}
# the .exe: 64-bit, small (the game's data stays in the player's ROM set), and needing no Visual C++ runtime
$bytes = [System.IO.File]::ReadAllBytes((Join-Path $out "donpachi.exe"))
$pe = [BitConverter]::ToInt32($bytes, 0x3C)
if ([BitConverter]::ToUInt16($bytes, $pe + 4) -ne 0x8664) { $problems += "donpachi.exe is not a 64-bit build" }
if ($bytes.Length -gt 8MB) { $problems += "donpachi.exe is $([int]($bytes.Length / 1MB)) MB: more than the program should be" }
$text = [System.Text.Encoding]::ASCII.GetString($bytes)
if ($text -match "(?i)vcruntime\d+d?\.dll|msvcp\d+d?\.dll|ucrtbased\.dll") {
    $problems += "donpachi.exe needs the Visual C++ runtime ($($Matches[0])): build with the static runtime"
}

if ($problems.Count -gt 0) {
    Remove-Item $out -Recurse -Force
    Write-Host "No package made:" -ForegroundColor Red
    $problems | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    exit 1
}

# (.NET's ZipFile, not Compress-Archive: Windows PowerShell's Compress-Archive writes the folders with "\", which
# other systems' unzip tools take as part of the file names)
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($out, $zip, [System.IO.Compression.CompressionLevel]::Optimal, $true)
Write-Host "Made $zip" -ForegroundColor Green
Get-ChildItem $out -Recurse -File | ForEach-Object {
    "  {0,-36} {1,10:N0} bytes" -f $_.FullName.Substring($out.Length + 1), $_.Length
}
