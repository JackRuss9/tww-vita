#!/usr/bin/env python3
"""Test the production animation routines using existing PC compile flags.

Run ./build-pc.sh first, then python3 check/test_animation.py. Compiles only
J3DAnimation.cpp and JMath.cpp, discards unrelated functions at link time,
and runs a native-width numerical test (no disc, display or 32-bit runtime).
"""
import json
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
commands = json.loads((root / 'build-pc/compile_commands.json').read_text())
with tempfile.TemporaryDirectory(prefix='tww-animation-test-') as tmp:
    objects = []
    flags = None
    for name in ('J3DAnimation.cpp', 'JMath.cpp'):
        entry = next(c for c in commands if Path(c['file']).name == name)
        args = shlex.split(entry['command'])
        out = args.index('-o')
        obj = str(Path(tmp) / (name + '.o'))
        args[out + 1] = obj
        args.remove('-m32')
        args += ['-ffunction-sections', '-fdata-sections']
        subprocess.run(args, cwd=entry['directory'], check=True)
        objects.append(obj)
        flags = args[:args.index('-o')]
    test = str(root / 'check/test_animation.cpp')
    test_obj = str(Path(tmp) / 'test.o')
    # The game's NDEBUG disables assertions; this test must keep them active.
    subprocess.run(flags + ['-UNDEBUG', '-o', test_obj, '-c', test], cwd=root, check=True)
    exe = str(Path(tmp) / 'test')
    subprocess.run(['g++', '-no-pie', '-Wl,--gc-sections', test_obj, *objects, '-lm', '-o', exe], check=True)
    subprocess.run([exe], check=True)
