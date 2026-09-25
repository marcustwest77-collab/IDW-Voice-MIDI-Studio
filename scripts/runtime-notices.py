"""Preserve installed distribution metadata and supplied license files for review."""
import importlib.metadata as metadata
import json
import shutil
import sys
from pathlib import Path
root = Path(sys.argv[1]); root.mkdir(parents=True, exist_ok=True)
inventory = []
for dist in metadata.distributions():
    name = dist.metadata['Name'] or 'unnamed'
    folder = root / name; folder.mkdir(exist_ok=True)
    inventory.append({'name': name, 'version': dist.version, 'license': dist.metadata.get('License', '')})
    (folder / 'METADATA.txt').write_text(dist.read_text('METADATA') or '', encoding='utf-8')
    for item in dist.files or []:
        if any(term in item.name.lower() for term in ('license', 'copyright', 'notice', 'copying')):
            source = Path(dist.locate_file(item))
            if source.is_file():
                target = folder / str(item).replace('..', '_').replace('\\', '_').replace('/', '_')
                shutil.copyfile(source, target)
(root / 'inventory.json').write_text(json.dumps(inventory, indent=2), encoding='utf-8')

python_license = Path(sys.base_prefix) / "LICENSE.txt"
if python_license.is_file(): shutil.copyfile(python_license, root / "Python-LICENSE.txt")
