$ErrorActionPreference = 'Stop'
python -m pip install pyinstaller==6.16.0 Pillow==11.3.0 -r Companion/requirements-speech.txt
if ($LASTEXITCODE -ne 0) { throw 'PyInstaller/speech installation failed' }
python scripts/fetch-speech-model.py
if ($LASTEXITCODE -ne 0) { throw 'Speech assets download failed' }
python -m PyInstaller --noconfirm --clean --onedir --windowed --name IDW-Audio-Lab --distpath audio-lab-dist --workpath audio-lab-build --specpath audio-lab-build --paths Companion --collect-all vosk --collect-all _sounddevice_data --add-data "speech-model;speech-model" --collect-all basic_pitch --collect-all librosa --collect-all onnxruntime --collect-all lazy_loader --exclude-module tensorflow --exclude-module coremltools --exclude-module tflite_runtime Companion/audio_lab.py
if ($LASTEXITCODE -ne 0) { throw 'Audio Lab packaging failed' }
$exe = (Resolve-Path 'audio-lab-dist/IDW-Audio-Lab/IDW-Audio-Lab.exe').Path
$report = Join-Path $PWD 'audio-lab-packaged-test.txt'
$preview = Join-Path $PWD 'IDW-V6-Take-Editor.png'
$speechTest = Join-Path $PWD 'speech-test.wav'
$isolated = Join-Path $env:RUNNER_TEMP 'idw-audio-lab-isolated'
New-Item -ItemType Directory -Force $isolated | Out-Null
$oldPath = $env:PATH
$oldPythonPath = $env:PYTHONPATH
try {
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    $env:PYTHONPATH = ''
    $process = Start-Process -FilePath $exe -ArgumentList @('--self-test', '--test-report', ('"' + $report + '"'), '--preview-path', ('"' + $preview + '"'), '--speech-test', ('"' + $speechTest + '"')) -WorkingDirectory $isolated -PassThru
    if (-not $process.WaitForExit(300000)) { $process.Kill(); throw 'Packaged Audio Lab test timed out' }
    if (Test-Path $report) { Get-Content $report }
    if ($process.ExitCode -ne 0 -or -not (Test-Path $report)) { throw 'Packaged Audio Lab test failed' }
    if (-not ((Get-Content $report -Raw) -match 'PASS packaged Tk GUI startup')) { throw 'GUI gate missing' }
} finally { $env:PATH = $oldPath; $env:PYTHONPATH = $oldPythonPath }
python scripts/runtime-notices.py audio-lab-dist/IDW-Audio-Lab/Third-Party
if ($LASTEXITCODE -ne 0) { throw 'Runtime notices failed' }
