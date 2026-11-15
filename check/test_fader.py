#!/usr/bin/env python3
"""Exercise production JUTFader state transitions without a graphics context."""
import json
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
entry = next(c for c in json.loads((root / 'build-pc/compile_commands.json').read_text())
             if Path(c['file']).name == 'JUTFader.cpp')
args = shlex.split(entry['command'])
flags = args[:args.index('-o')]
flags.remove('-m32')
flags += ['-UNDEBUG']
with tempfile.TemporaryDirectory(prefix='tww-fader-') as tmp:
    header = Path(tmp) / 'JSystem/J2DGraph/J2DOrthoGraph.h'
    header.parent.mkdir(parents=True)
    header.write_text('''#include "JSystem/JGeometry.h"
#include "JSystem/JUtility/TColor.h"
class J2DOrthoGraph {
public:
    void setColor(JUtility::TColor) {}
    void fillBox(const JGeometry::TBox2<float>&) {}
};
''')
    flags[1:1] = ['-I' + tmp]
    objects = []
    for i, source in enumerate((entry['file'], root / 'check/test_fader.cpp')):
        obj = str(Path(tmp) / ('%d.o' % i))
        subprocess.run(flags + ['-c', str(source), '-o', obj], cwd=entry['directory'], check=True)
        objects.append(obj)
    exe = str(Path(tmp) / 'test')
    subprocess.run(['g++', '-no-pie', *objects, '-o', exe], check=True)
    subprocess.run([exe], check=True)
