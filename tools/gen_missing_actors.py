#!/usr/bin/env python3
"""List actor profiles whose cPhs_State creation entry is still a decomp stub."""
import re
import sys
from pathlib import Path

root, output = map(Path, sys.argv[1:])
profiles = {}
sys.dont_write_bytecode = True
from stub_returns import MARK, body_end

pattern = re.compile(r'cPhs_State\s+[\w:]*[Cc]reate\s*\([^)]*\)\s*\{')

def has_missing_creator(text):
    for match in pattern.finditer(text):
        if not re.match(r'\s*/\* Nonmatching \*/', text[match.end():]):
            continue
        end = body_end(text, match.end() - 1)
        if end < 0:
            raise ValueError("unbalanced actor creation body: " + text[match.start():match.end()])
        body = text[match.end():end]
        plain = re.sub(r'/\*.*?\*/|//[^\n]*', '', body, flags=re.S).strip()
        if MARK in body or ('Nonmatching' in body and plain in ('', 'return {};')):
            return True
    return False

for source in sorted((root / 'src/d/actor').glob('*.cpp')):
    text = source.read_text()
    if has_missing_creator(text):
        for profile in re.findall(r'/\* Proc Name\s*\*/\s*(fpcNm_\w+)', text):
            profiles[profile] = source.name
content = '// Generated: creation callbacks not yet decompiled.\n' + ''.join(
    f'case {profile}: // {source}\n' for profile, source in sorted(profiles.items()))
if not output.exists() or output.read_text() != content:
    output.write_text(content)
print(f'gen_missing_actors: {len(profiles)} profiles with missing creation code')
