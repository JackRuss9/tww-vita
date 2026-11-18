# Departure replay from actual earned shield state: verifies departure.stb plays to its end,
# grants EventFlag 0x2401 and hands control back on the pirate ship (A_umikz).
set pagination off
set confirm off
set print static-members off
set python print-stack full
handle SIGUSR1 SIGUSR2 SIGPIPE nostop noprint
python
import gdb,struct,os
assert os.environ.get('TWW_DEPARTURE_CHECKPOINT'), 'Supply actual earned shield state'
keys=int(gdb.parse_and_eval("&'(anonymous namespace)::g_key'"))
class Resume(gdb.Breakpoint):
 def __init__(self):
  super().__init__('daPy_lk_c::execute',internal=True);self.age=0;self.last=None;self.maxframe=0;self.ended=False;self.calm=0
 def stop(self):
  self.age+=1
  try:p=gdb.parse_and_eval('this')
  except gdb.error:p=gdb.parse_and_eval('(daPy_lk_c*)i_this')
  if self.age==180:
   saved=gdb.parse_and_eval('g_dComIfG_gameInfo.save.mSavedata')
   raw=open(os.environ['TWW_DEPARTURE_CHECKPOINT'],'rb').read();assert len(raw)==saved.type.sizeof
   gdb.selected_inferior().write_memory(int(saved.address),raw)
   req=int(gdb.parse_and_eval('&g_dComIfG_gameInfo.play.mNextStage'))
   gdb.selected_inferior().write_memory(req,struct.pack('<8shbbbb',b'sea',205,44,10,1,0))
   print('[departure] actual earned shield state restored; replay of native Tetra destination sea205/44/10')
  demo=int(p['mDemo']['mDemoType'])
  # Advance the cinematic's dialogue with A only while a demo is running.
  gdb.selected_inferior().write_memory(keys+44,bytes([self.age>180 and demo and 70<=self.age%120<95]))
  stage=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCurStage.mName').string()
  cam=gdb.parse_and_eval('((camera_process_class*)g_dComIfG_gameInfo.play.mCameraInfo[0].mpCamera)->mCamera')
  staff=int(cam['mEventData']['mStaffIdx']);cut='none'
  events=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtManager.mList')
  if staff>=0 and int(events['mStaffP']):
   ci=int(events['mStaffP'][staff]['mCurCutIdx'])
   if ci>=0:cut=events['mCutP'][ci]['mName'].string()
  event=int(gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtCtrl.mEventId'))
  dm=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mDemo')
  frame=int(dm['mFrame']);mode=int(dm['mMode'])
  if stage=='sea' and event==115:
   self.maxframe=max(self.maxframe,frame)
   if mode==2:self.ended=True
  state=(stage,demo,cut,event)
  if state!=self.last:
   print('[departure] stage/demo/camera/event=%s'%(state,));self.last=state
  if self.age%120==0:
   print('[departure] age=%d pos=%s package frame=%d (no message %d) mode=%d'%(self.age,tuple(float(p['current']['pos'][a]) for a in ('x','y','z')),frame,int(dm['mFrameNoMsg']),mode))
  if stage=='A_umikz' and event==-1 and not demo and mode==0:
   self.calm+=1
  else:
   self.calm=0
  if self.calm==60:
   flag=int(gdb.parse_and_eval('g_dComIfG_gameInfo.save.mSavedata.mEvent.mFlags[0x24]'))&1
   assert self.ended,'departure package never reached its end'
   assert self.maxframe>2000,'departure package stopped early at frame %d'%self.maxframe
   assert flag,'departure EventFlag 0x2401 not granted'
   print('[departure] DEPARTURE VERIFIED: departure.stb played to frame %d, EventFlag 0x2401 set, A_umikz reached with no event or demo for 60 frames; Link at %s'%(self.maxframe,tuple(float(p['current']['pos'][a]) for a in ('x','y','z'))))
   gdb.set_convenience_variable('tww_probe_success',1);return True
  return False
Resume()
end
source ../tools/crash.gdb
python
import sys
sys.dont_write_bytecode=True
sys.path.insert(0,'../tools')
import orca_story
orca_story.describe()
end
