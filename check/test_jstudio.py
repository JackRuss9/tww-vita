#!/usr/bin/env python3
"""Exercise the production STB payload parser with synthetic big-endian data."""
import json
from pathlib import Path
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parent.parent
commands = json.loads((root / 'build-pc/compile_commands.json').read_text())
entry = next(c for c in commands if Path(c['file']).name == 'stb-data-parse.cpp')
args = shlex.split(entry['command'])
flags = args[:args.index('-o')]
flags.remove('-m32')
flags += ['-ffunction-sections', '-fdata-sections', '-UNDEBUG']
with tempfile.TemporaryDirectory(prefix='tww-stb-test-') as tmp:
    objects = []
    for index, source in enumerate((entry['file'], root / 'tww/src/JSystem/JStudio/JStudio/stb-data.cpp', root / 'check/test_jstudio.cpp')):
        obj = str(Path(tmp) / ('%d.o' % index))
        subprocess.run(flags + ['-c', str(source), '-o', obj], cwd=entry['directory'], check=True)
        objects.append(obj)
    exe = str(Path(tmp) / 'test')
    subprocess.run(['g++', '-no-pie', '-Wl,--gc-sections', *objects, '-o', exe], check=True)
    subprocess.run([exe], check=True)
