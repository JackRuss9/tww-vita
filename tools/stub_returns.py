#!/usr/bin/env python3
"""Gives the decomp's still-empty functions a return value.

Functions not decompiled yet have an empty body ("/* Nonmatching */"). On
the GameCube one returning a value would hand back whatever was left in a
register; GCC treats running off the end of a non-void function as
impossible and drops the return altogether, so the call runs on into the
next function's code (check1stDynamicWave fell into JKRExpHeap::createRoot).
Each such body gets `return {};` (zero, NULL, an empty value).

Run on tww/src after the patches (tools/fetch.sh); idempotent.

  stub_returns.py <tww dir>
"""
import os, re, sys

MARK = "return {}; // tww-vita: not decompiled yet"

# A function definition whose body holds only comments and whitespace.
DEF = re.compile(
    r'(?P<sig>^[^\n;{}#/][^;{}]*?\)\s*(?:const\s*)?)\{(?P<body>(?:\s|/\*.*?\*/|//[^\n]*)*)\}',
    re.M | re.S)

def return_type(sig):
    """The declared return type, or None for constructors, destructors,
    operators that need no value, and things that are not functions."""
    # Only the declaration itself: drop anything up to the last end of a
    # comment or preprocessor line (a match can start inside the file's
    # opening comment and run over the #includes).
    cut = max(sig.rfind('*/') + 2 if '*/' in sig else 0,
              max((m.end() for m in re.finditer(r'^\s*#[^\n]*\n', sig, re.M)), default=0))
    sig = sig[cut:]
    s = re.sub(r'/\*.*?\*/|//[^\n]*', ' ', sig, flags=re.S)
    s = ' '.join(s.split())
    m = re.match(r'(?:template\s*<[^>]*>\s*)?(.*?[\s*&])?\s*((?:[A-Za-z_][\w]*(?:<[^()]*>)?::)*~?[A-Za-z_][\w]*|(?:[A-Za-z_][\w]*::)*operator\s*[^\s(]+)\s*\((.*)\)\s*(const)?\s*$', s)
    if not m:
        return None
    head, name = (m.group(1) or '').strip(), m.group(2).strip()
    last = name.split('::')[-1]
    if not head or '~' in last:
        return None  # constructor, destructor
    if len(name.split('::')) >= 2 and name.split('::')[-1] == name.split('::')[-2]:
        return None  # Class::Class
    head = re.sub(r'\b(static|inline|virtual|extern|"C"|explicit|friend)\b', ' ', head).strip()
    for kw in ('if', 'while', 'for', 'switch', 'return', 'else', 'do', 'catch'):
        if last == kw or head.endswith(kw):
            return None
    return ' '.join(head.split())

# Any function definition starting at column 0 with its "{" ending the line
# (the decomp's style); the body is found by matching braces.
DEF_ANY = re.compile(r'(?P<sig>^[A-Za-z_][^;{}#]*?\)\s*(?:const\s*)?)\{[ \t]*(?://[^\n]*)?$', re.M | re.S)

def body_end(t, i):
    """Index of the "}" closing the brace at t[i], skipping comments,
    strings and character literals; -1 if unbalanced."""
    depth, n = 0, len(t)
    while i < n:
        c = t[i]
        if c == '/' and t.startswith('//', i):
            i = t.find('\n', i)
            if i < 0:
                return -1
        elif c == '/' and t.startswith('/*', i):
            i = t.find('*/', i + 2)
            if i < 0:
                return -1
            i += 1
        elif c in '"\'':
            j = i + 1
            while j < n and t[j] != c:
                j += 2 if t[j] == '\\' else 1
            i = j
        elif c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1

def last_statement_returns(body):
    s = re.sub(r'/\*.*?\*/|//[^\n]*', ' ', body, flags=re.S).strip()
    if not s:
        return False
    # the last statement: after the last ";" or "}" before the end
    tail = s.rstrip()
    m = re.search(r'(?:^|[;{}])\s*(return\b[^;]*;)\s*$', tail, re.S)
    return bool(m)

def process_partial(path, t):
    """Non-void functions not decompiled yet (Nonmatching/TODO) whose body has
    code but can run off its end: `return {};` goes before the closing brace."""
    out, pos, n = [], 0, 0
    for m in DEF_ANY.finditer(t):
        if m.start() < pos:
            continue
        open_i = m.end() - 1
        while t[open_i] != '{':
            open_i -= 1
        close_i = body_end(t, open_i)
        if close_i < 0:
            continue
        body = t[open_i + 1:close_i]
        if MARK in body or not re.search(r'Nonmatching|TODO', body):
            continue
        rt = return_type(m.group('sig'))
        if rt is None or rt == 'void' or rt.endswith('&'):
            continue
        if last_statement_returns(body):
            continue
        if os.environ.get('STUB_RETURNS_VERBOSE'):
            print('%s: (partial) %s | %s' % (os.path.basename(path), rt, ' '.join(m.group('sig').split())[:90]))
        out.append(t[pos:close_i])
        out.append('    ' + MARK + '\n')
        pos = close_i
        n += 1
    if n:
        out.append(t[pos:])
        return ''.join(out), n
    return t, 0

def process(path):
    t = open(path, encoding='utf-8', errors='surrogateescape').read()
    t, n_partial = process_partial(path, t)
    if n_partial:
        open(path, 'w', encoding='utf-8', errors='surrogateescape').write(t)
    out, pos, n = [], 0, 0
    for m in DEF.finditer(t):
        body = m.group('body')
        if MARK in body:
            continue
        rt = return_type(m.group('sig'))
        if rt is None or rt in ('void',) or rt.endswith('&'):
            continue
        if not re.search(r'Nonmatching|TODO', body):
            continue  # an empty body on purpose
        if os.environ.get('STUB_RETURNS_VERBOSE'):
            print('%s: %s | %s' % (os.path.basename(path), rt, ' '.join(m.group('sig').split())[:90]))
        out.append(t[pos:m.end('body')])
        ind = '    '
        out.append(ind + MARK + '\n')
        pos = m.end('body')
        n += 1
    if n:
        out.append(t[pos:])
        open(path, 'w', encoding='utf-8', errors='surrogateescape').write(''.join(out))
    return n + n_partial

def main():
    src = os.path.join(sys.argv[1], 'src')
    total, files = 0, 0
    for root, _, names in os.walk(src):
        if '/dolphin' in root or 'PowerPC_EABI' in root or 'TRK' in root:
            continue
        for f in names:
            if f.endswith(('.cpp', '.inc', '.c')):
                k = process(os.path.join(root, f))
                if k:
                    total += k
                    files += 1
    print('stub_returns: %d empty functions given a return value, in %d files' % (total, files))

if __name__ == '__main__':
    main()
