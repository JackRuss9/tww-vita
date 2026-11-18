# Diagnostic replay from earned post-rescue savedata; no menu/forest-route proof.
set pagination off
set confirm off
set print static-members off
set python print-stack full
handle SIGUSR1 SIGUSR2 SIGPIPE nostop noprint
python
import gdb,struct,os,math,sys
sys.dont_write_bytecode=True
sys.path.insert(0,os.path.abspath('../tools'))
from beach_camera import install,Done
install()
assert os.environ.get('TWW_BEACH_CHECKPOINT'), 'Supply earned post-rescue savedata'
keys=int(gdb.parse_and_eval("&'(anonymous namespace)::g_key'"))
class Resume(gdb.Breakpoint):
 def __init__(self):
  super().__init__('daPy_lk_c::execute',internal=True);self.age=0;self.seen=False;self.free=0;self.origin=None
 def stop(self):
  self.age+=1
  try:p=gdb.parse_and_eval('this')
  except gdb.error:p=gdb.parse_and_eval('(daPy_lk_c*)i_this')
  if self.age==180:
   saved=gdb.parse_and_eval('g_dComIfG_gameInfo.save.mSavedata')
   raw=open(os.environ['TWW_BEACH_CHECKPOINT'],'rb').read();assert len(raw)==saved.type.sizeof
   gdb.selected_inferior().write_memory(int(saved.address),raw)
   request=int(gdb.parse_and_eval('&g_dComIfG_gameInfo.play.mNextStage'))
   gdb.selected_inferior().write_memory(request,struct.pack('<8shbbbb',b'sea',0,44,-1,1,0))
   print('[beach] actual earned rescue state restored; diagnostic sea reload, no quest flag grants')
  demo=int(p['mDemo']['mDemoType'])
  gdb.selected_inferior().write_memory(keys+44,bytes([self.age>180 and demo and 70<=self.age%120<95]))
  event=int(gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtCtrl.mEventId'))
  self.seen |= self.age>180 and event==250 and bool(demo)
  if self.seen and not demo and event!=250:
   if self.origin is None:self.origin=tuple(float(p['current']['pos'][a]) for a in ('x','y','z'))
   self.free+=1
   gdb.selected_inferior().write_memory(keys+26,bytes([self.free<30]))
   if self.free==30:
    xyz=tuple(float(p['current']['pos'][a]) for a in ('x','y','z'))
    distance=math.hypot(xyz[0]-self.origin[0],xyz[2]-self.origin[2])
    assert distance>10,distance
    assert {10,20}.issubset(Done.completed),Done.completed
    checkpoint=os.environ.get('TWW_BEACH_SAVE_CHECKPOINT')
    if checkpoint:
     saved=gdb.parse_and_eval('g_dComIfG_gameInfo.save.mSavedata')
     with open(checkpoint,'wb') as stream:stream.write(bytes(gdb.selected_inferior().read_memory(int(saved.address),saved.type.sizeof)))
    print('[beach] SUCCESS yuukaigo ended naturally and Link walks %.3f units'%distance)
    gdb.set_convenience_variable('tww_probe_success',1);return True
  if self.age%120==0:print('[beach] age=%d demo=%d proc=%s pos=%s'%(self.age,demo,p['mCurProc'],tuple(float(p['current']['pos'][a]) for a in ('x','y','z'))))
  return False
class Transition(gdb.Breakpoint):
 def __init__(self):super().__init__('dCamera_c::uniformTransEvCamera',internal=True)
 def stop(self):
  c=gdb.parse_and_eval('this')
  if int(c['m11C'])==0:
   print('[beach] UNITRANS entry')
   if os.environ.get('TWW_BEACH_STOP_CAMERA')=='1':return True
  return False
Resume();Transition()
end
source ../tools/crash.gdb
python
import sys
sys.dont_write_bytecode=True
sys.path.insert(0,'../tools')
import orca_story
orca_story.describe()
end

python
import beach_camera,importlib
importlib.reload(beach_camera)
beach_camera.describe()
end
