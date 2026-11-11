# run-pc.sh runs the game under gdb with this script: on a crash, the
# backtrace, then (since a jump through a bad pointer leaves gdb no frames)
# every code address on the stack, which recovers the callers, then all
# threads.
set pagination off
set confirm off
handle SIGUSR1 SIGUSR2 SIGPIPE nostop noprint
run
echo \n== crash: backtrace\n
bt 40
echo \n== crash: registers and code\n
info registers eip esp ebp eax ecx edx
python
try:
    gdb.execute("x/6i $pc")
except gdb.error as e:
    print("  (no code at the crash address: %s)" % e)
end
echo \n== crash: code addresses on the stack (callers, newest first)\n
python
import gdb
inf = gdb.selected_inferior()
sp = int(gdb.parse_and_eval("$esp"))
shown = 0
for i in range(1024):
    try:
        w = int.from_bytes(inf.read_memory(sp + 4 * i, 4).tobytes(), "little")
    except gdb.MemoryError:
        break
    s = gdb.execute("info symbol 0x%x" % w, to_string=True).strip()
    if "No symbol" in s or "in section .text" not in s:
        continue
    print("  [esp+0x%03x] 0x%08x %s" % (4 * i, w, s.split(" in section")[0]))
    shown += 1
    if shown >= 40:
        break
end
echo \n== all threads\n
thread apply all bt 8
echo \n== recent disc files and game context\n
python
try:
    gdb.execute("call (void)port_crash_describe_context()")
except gdb.error as e:
    print("  (not available: %s)" % e)
end
