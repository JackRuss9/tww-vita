#!/usr/bin/env python3
"""Test production TWW constants against the native DSP decoder, without disc data."""
import json
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
commands = json.loads((root / 'build-pc/compile_commands.json').read_text())
entry = next(c for c in commands if Path(c['file']).name == 'JASDriverTables.cpp')
args = shlex.split(entry['command'])
flags = args[:args.index('-o')]
flags.remove('-m32')
flags += ['-UNDEBUG', '-I' + str(root / 'sms-port/platform/audio')]
with tempfile.TemporaryDirectory(prefix='tww-dsp-tables-') as tmp:
    objects = []
    for i, source in enumerate((entry['file'], root / 'check/test_dsp_tables.cpp',
                                root / 'sms-port/platform/audio/dsp_mixer.cpp')):
        obj = str(Path(tmp) / ('%d.o' % i))
        compile_flags = flags if i == 0 else ['g++', '-std=c++11', '-O2',
            '-I' + str(root / 'port/include'), '-I' + str(root / 'sms-port/platform/audio')]
        subprocess.run(compile_flags + ['-c', str(source), '-o', obj], cwd=entry['directory'], check=True)
        objects.append(obj)
    exe = str(Path(tmp) / 'test')
    subprocess.run(['g++', '-no-pie', *objects, '-lm', '-o', exe], check=True)
    subprocess.run([exe], check=True)
