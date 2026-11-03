#!/usr/bin/env python3
"""Compiler launcher for the actor modules (CMake's CXX_COMPILER_LAUNCHER):
compiles the file, then moves its static constructors (.init_array, .ctors)
into a section of the module's own, tww_ctors_<module>.

On the console a module's constructors run when the game links it (when an
actor of that kind is first created), with the game already running; linked
into the program they would run before main() (daNpc_Tc_HIO_c's reads the
save file). DynamicModuleControl::do_link runs them instead (rel_ctors.cpp,
through the linker's __start_/__stop_ symbols for the section).

  rel_cc.py <objcopy> <compiler> <args...>
"""
import os, subprocess, sys

def main():
    objcopy = sys.argv[1]
    cmd = sys.argv[2:]
    r = subprocess.call(cmd)
    if r != 0:
        return r
    out = src = None
    for i, a in enumerate(cmd):
        if a == "-o" and i + 1 < len(cmd):
            out = cmd[i + 1]
        if a == "-c" and i + 1 < len(cmd):
            src = cmd[i + 1]
    if not out or not src or "-c" not in cmd:
        return 0
    name = os.path.splitext(os.path.basename(src))[0]
    sec = "tww_ctors_" + name
    return subprocess.call([objcopy, "--rename-section", ".init_array=" + sec,
                            "--rename-section", ".ctors=" + sec, out])

if __name__ == "__main__":
    sys.exit(main())
