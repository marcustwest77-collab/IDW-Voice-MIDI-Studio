param([string]$JucePath = "", [string]$BuildDirectory = "build-win-6")
$ErrorActionPreference = "Stop"
Set-Location (Split-Path $PSScriptRoot -Parent)
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vs = & $vswhere -latest -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vs) {
            $candidate = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
            if (Test-Path (Join-Path $candidate 'cmake.exe')) { $env:PATH = "$candidate;$env:PATH" }
        }
    }
}
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { throw 'Install Visual Studio 2022 Desktop development with C++, including CMake tools and a Windows SDK. Then retry.' }
$configure = @('-S', '.', '-B', $BuildDirectory, '-G', 'Visual Studio 17 2022', '-A', 'x64', '-DBUILD_TESTING=ON')
if ($JucePath) { $configure += "-DIDW_JUCE_PATH=$JucePath" }
& cmake @configure
if ($LASTEXITCODE -ne 0) { throw 'Configuration failed. Check C++ tools, Windows SDK, Git and internet access, or supply -JucePath.' }
& cmake --build $BuildDirectory --config Release --parallel 3
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed; no release package created.' }
& ctest --test-dir $BuildDirectory -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed; no release package created.' }
$artefacts = Join-Path $BuildDirectory 'IDWVoiceMIDIStudio_artefacts\Release'
$exe = Join-Path $artefacts 'Standalone\IDW Voice MIDI Studio.exe'
$vst = Join-Path $artefacts 'VST3\IDW Voice MIDI Studio.vst3'
if (-not (Test-Path $exe) -or -not (Test-Path $vst)) { throw 'Build outputs missing.' }
$release = Join-Path (Get-Location) ('IDW-V6-Windows-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path "$release\Windows\Standalone", "$release\Windows\VST3" | Out-Null
Copy-Item $exe "$release\Windows\Standalone"
Copy-Item $vst "$release\Windows\VST3" -Recurse
Copy-Item 'Companion' "$release\Companion" -Recurse
Copy-Item 'Integrations' "$release\Integrations" -Recurse
Copy-Item 'docs' "$release\docs" -Recurse
Copy-Item 'START-HERE.html' $release
Copy-Item 'START-HERE.txt' $release
New-Item -ItemType Directory -Path "$release\Previews" | Out-Null
Get-ChildItem "$BuildDirectory\IDW-V6-*.png" | Copy-Item -Destination "$release\Previews"
New-Item -ItemType Directory -Path "$release\ThirdPartyNotices" | Out-Null
$juceLicense = if ($JucePath) { Join-Path $JucePath 'LICENSE.md' } elseif (Test-Path 'ThirdParty/JUCE/LICENSE.md') { 'ThirdParty/JUCE/LICENSE.md' } else { Join-Path $BuildDirectory '_deps/juce-src/LICENSE.md' }
Copy-Item $juceLicense "$release\ThirdPartyNotices\JUCE-LICENSE.md"
Copy-Item "$BuildDirectory\Testing\Temporary\LastTest.log" "$release\Build-Tests.log"
'@echo off', 'start "IDW V6" "%~dp0Windows\Standalone\IDW Voice MIDI Studio.exe"' | Set-Content "$release\Run-IDW.cmd"
Compress-Archive -Path $release -DestinationPath "$release.zip"
Write-Host "SUCCESS: $release.zip"
Write-Host "Run: $release\Run-IDW.cmd"
Write-Host 'No installed plugin has been replaced. Follow the guide for installation and a live microphone test.'
