# Isolated keyboard regression: I first, swim + I, natural drowning/restart,
# and movement after respawn. No positions, timer or story flags are injected.
# ./tools/probe-pc.sh --outset --headless --deterministic --seconds 300 --gdb "$PWD/tools/camera_recovery.gdb"
set pagination off
set confirm off
handle SIGUSR1 SIGUSR2 SIGPIPE nostop noprint
python
import gdb,struct,math
keybase=int(gdb.parse_and_eval("&'(anonymous namespace)::g_key'"))
n=0
swim_age=0
restart=False
recovery_age=0
origin=None
last=None
class Restart(gdb.Breakpoint):
 def stop(self):
  global restart
  if swim_age and not restart:
   restart=True
   print('[recovery] native startRestartRoom after swimming')
  return False
class Done(gdb.FinishBreakpoint):
 def __init__(self,address):
  super().__init__(gdb.newest_frame(),internal=True)
  self.address=address
 def stop(self):
  global swim_age,recovery_age,origin,last
  c=gdb.parse_and_eval('((dCamera_c*)0x%x)'%self.address)
  player=c['mpPlayerActor'].cast(gdb.lookup_type('daPy_lk_c').pointer())
  if not player:return False
  proc=int(player['mCurProc']);mode=int(c['mCurMode'])
  demo=int(player['mDemo']['mDemoType'])
  swimming=proc in (0x35,0x36,0x37)
  if swimming:swim_age+=1
  view=tuple(float(c[f][x]) for f in ('mEye','mCenter') for x in ('x','y','z'))
  assert all(math.isfinite(x) and abs(x)<1000000 for x in view)
  assert math.isfinite(float(c['mFovy'])) and 0<float(c['mFovy'])<180
  state=(mode,int(c['mCurStyle']),proc,demo)
  if state!=last or n%120==0:
   print('[recovery] frame=%d mode/style/proc/demo=%s swim=%d restart=%s'%(n,state,swim_age,restart));last=state
  if 190<n<235:assert mode==12
  if 10<swim_age<55 and not restart:assert mode==12
  if recovery_age or (restart and not swimming and proc in (4,6) and demo==0):
   recovery_age+=1
   xyz=tuple(float(player['current']['pos'][x]) for x in ('x','y','z'))
   if recovery_age==30:origin=xyz
   if recovery_age>=60 and origin:
    distance=sum((xyz[i]-origin[i])**2 for i in (0,2))**.5
    print('[recovery] post-respawn movement=%.3f'%distance)
    assert distance>10
    print('[recovery] SUCCESS: I-first, I while swimming, natural restart, finite camera and control return')
    gdb.set_convenience_variable('tww_probe_success',1)
    return True
  return False
class Camera(gdb.Breakpoint):
 def stop(self):
  global n
  n+=1
  keys=[]
  if restart:
   if 30<=recovery_age<90:keys=[26]
  elif swim_age:
   keys=[22]+([82] if swim_age<60 else [])
  elif 180<=n<240:keys=[82]
  elif n>=300:keys=[22]
  for code in (82,22,26):gdb.selected_inferior().write_memory(keybase+code,bytes([code in keys]))
  sp=int(gdb.parse_and_eval('$esp'))
  address=struct.unpack('<I',bytes(gdb.selected_inferior().read_memory(sp+4,4)))[0]
  Done(address)
  return False
Restart('daPy_lk_c::startRestartRoom')
Camera('*0x%x'%int(gdb.parse_and_eval("&'dCamera_c::Run()'")))
end
source ../tools/crash.gdb
