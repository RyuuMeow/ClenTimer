param([Parameter(Mandatory)][string]$QtDir)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$stage = Join-Path $root 'stage'
$dist = Join-Path $root 'dist'
if (Test-Path $stage) { throw 'Use an empty staging path. Move the existing stage directory before packaging.' }
New-Item -ItemType Directory -Force $stage,$dist | Out-Null
Copy-Item "$root/build/Release/ClenTimer.exe" $stage
Copy-Item "$root/assets/*" $stage -Recurse
foreach ($name in 'Core','Gui','Widgets','Svg') { Copy-Item "$QtDir/bin/Qt6$name.dll" $stage }
foreach ($plugin in 'platforms/qwindows.dll','iconengines/qsvgicon.dll') {
    $folder = Join-Path $stage (Split-Path $plugin)
    New-Item -ItemType Directory -Force $folder | Out-Null
    Copy-Item "$QtDir/plugins/$plugin" $folder
}
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$crt = Get-ChildItem "$vs/VC/Redist/MSVC/*/x64/Microsoft.VC143.CRT" -Directory | Sort-Object FullName -Descending | Select-Object -First 1
if (!$crt) { throw 'MSVC x64 redistributable not found.' }
Copy-Item "$($crt.FullName)/*.dll" $stage
& "${env:ProgramFiles(x86)}/Inno Setup 6/ISCC.exe" /Q "$PSScriptRoot/setup.iss"
if ($LASTEXITCODE) { throw 'Installer compilation failed.' }
New-Item -ItemType File "$stage/portable.mode" | Out-Null
Compress-Archive -Path "$stage/*" -DestinationPath "$dist/ClenTimer-1.10-x64-Portable.zip" -Force
