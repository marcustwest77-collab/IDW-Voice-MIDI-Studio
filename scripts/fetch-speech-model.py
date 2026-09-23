"""Build-time downloads only. The packaged app never downloads speech assets."""
from pathlib import Path
import hashlib
import json
import urllib.request
import zipfile
root=Path(__file__).resolve().parents[1]
assets=root/'speech-model';assets.mkdir(exist_ok=True)
url='https://alphacephei.com/vosk/models/vosk-model-small-en-us-0.15.zip'
archive=root/'speech-model.zip'
with urllib.request.urlopen(url,timeout=120) as response,archive.open('wb') as output:
    total=0
    while True:
        chunk=response.read(1024*1024)
        if not chunk:break
        total+=len(chunk)
        if total>100*1024*1024:raise RuntimeError('Unexpected speech-model size')
        output.write(chunk)
assert hashlib.sha256(archive.read_bytes()).hexdigest()=='30f26242c4eb449f948e42cb302dd7a686cb29a3423a8367f99ff41780942498', 'Speech model identity changed'
with zipfile.ZipFile(archive) as package:
    for info in package.infolist():
        target=(assets/info.filename).resolve()
        if not target.is_relative_to(assets.resolve()):raise RuntimeError('Invalid model archive path')
    if package.testzip() is not None:raise RuntimeError('Speech archive failed CRC verification')
    package.extractall(assets)
assert (assets/'vosk-model-small-en-us-0.15/am/final.mdl').is_file()
(assets/'MODEL-NOTICE.json').write_text(json.dumps({'model':'vosk-model-small-en-us-0.15','source':url,'license':'Apache-2.0 per https://alphacephei.com/vosk/models','archive_sha256':hashlib.sha256(archive.read_bytes()).hexdigest()},indent=2))
with urllib.request.urlopen('https://www.apache.org/licenses/LICENSE-2.0.txt',timeout=60) as response:(assets/'APACHE-2.0-LICENSE.txt').write_bytes(response.read())
fixture=root/'speech-test.wav'
with urllib.request.urlopen('https://raw.githubusercontent.com/alphacep/vosk-api/master/python/example/test.wav',timeout=60) as response:fixture.write_bytes(response.read())
assert hashlib.sha256(fixture.read_bytes()).hexdigest()=='dcfea5712c43a43ba7ae8083afb39d36993e5a69c46e88b68aaa72b65cb615bb', 'Speech fixture identity changed'
print('Speech model archive SHA256:',hashlib.sha256(archive.read_bytes()).hexdigest())
print('Official speech fixture SHA256:',hashlib.sha256(fixture.read_bytes()).hexdigest())
