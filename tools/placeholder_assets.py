#!/usr/bin/env python3
"""For the host compile check only: stand-ins for the small data files the
game's code embeds (textures, display lists, vertex tables), which the real
build extracts from your own disc image (tww/orig/GZLE01).

Each stand-in is all zeros, at the size the symbol has in the game, written
in the same shape as the real header (the decomp's converters are run on
the zeros), so the code compiles. Nothing from the game is in them; a
program built with them would draw nothing where they are used.

  placeholder_assets.py <tww dir> <out include dir>
"""
import os, re, subprocess, sys
import yaml

def sizes(symbols_txt):
    res = {}
    for line in open(symbols_txt):
        m = re.match(r'\s*(\S+)\s*=\s*(\.\w+):(0x[0-9A-Fa-f]+);.*size:0x([0-9A-Fa-f]+)', line)
        if m:
            size = int(m.group(4), 16)
            res.setdefault(m.group(1), size)
            res["%s!%s:0x%08x" % (m.group(1), m.group(2), int(m.group(3), 16))] = size
    return res

def main():
    tww, out = sys.argv[1], sys.argv[2]
    cfg = yaml.safe_load(open(os.path.join(tww, "config/GZLE01/config.yml")))
    units = [cfg] + cfg.get("modules", [])
    n = 0
    for u in units:
        syms = sizes(os.path.join(tww, u["symbols"]))
        for a in u.get("extract", []) or []:
            sym = a["symbol"]
            size = syms.get(sym) or syms.get(sym.lower() if "!" not in sym else
                                             sym.split("!")[0] + "!" + sym.split("!")[1].lower())
            if size is None:
                print("no size for", sym); continue
            name = a.get("rename") or sym.split("!")[0]
            hdr = os.path.join(out, a["header"])
            os.makedirs(os.path.dirname(hdr), exist_ok=True)
            ctype = a.get("custom_type")
            if ctype:
                # named as the real one: matDL_dis.py names its macro after it
                tmp = os.path.join(out, ".zeros")
                os.makedirs(tmp, exist_ok=True)
                binf = os.path.join(tmp, os.path.basename(a["binary"]))
                open(binf, "wb").write(b"\0" * size)
                scope = (a.get("custom_data") or {}).get("scope", "local")
                tool = "matDL_dis.py" if ctype == "matDL" else "extract_model_data.py"
                cmd = [sys.executable, os.path.join(tww, "tools/converters", tool), binf, hdr,
                       "--symbol", name, "--scope", scope]
                if ctype != "matDL":
                    cmd += ["--type", ctype]
                r = subprocess.run(cmd, capture_output=True, text=True)
                if r.returncode != 0 and ctype == "matDL":
                    # (the converter cannot read all-zero commands)
                    macro = os.path.splitext(os.path.basename(a["binary"]))[0]
                    pre = "static " if scope == "local" else ""
                    open(hdr, "w").write("#define %s(TEX_NAME) ALIGN_DECL(32, %su8 %s[0x%X]) = {0};\n"
                                         % (macro, pre, name, size))
                elif r.returncode != 0:
                    print("converter failed for", sym, r.stderr.strip().splitlines()[-1:])
                os.remove(binf)
            elif a.get("header_type") == "none":
                open(hdr, "w").write(", ".join(["0x00"] * size) + "\n")
            else:
                open(hdr, "w").write("static u8 %s[0x%X] ATTRIBUTE_ALIGN(32) = {0};\n" % (name, size))
            n += 1
    print("placeholder_assets: %d stand-ins in %s" % (n, out))

if __name__ == "__main__":
    main()
