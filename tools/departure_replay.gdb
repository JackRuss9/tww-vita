# Diagnostic departure replay; expected-blocker mode verifies failure handling, not story completion.
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
  super().__init__('daPy_lk_c::execute',internal=True);self.age=0;self.last=None
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
  gdb.selected_inferior().write_memory(keys+44,bytes([self.age>180 and demo and 70<=self.age%120<95]))
  stage=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCurStage.mName').string()
  cam=gdb.parse_and_eval('((camera_process_class*)g_dComIfG_gameInfo.play.mCameraInfo[0].mpCamera)->mCamera')
  staff=int(cam['mEventData']['mStaffIdx']);cut='none'
  events=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtManager.mList')
  if staff>=0 and int(events['mStaffP']):
   ci=int(events['mStaffP'][staff]['mCurCutIdx'])
   if ci>=0:cut=events['mCutP'][ci]['mName'].string()
  state=(stage,demo,cut,int(gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtCtrl.mEventId')))
  if state!=self.last:
   print('[departure] stage/demo/camera/event=%s'%(state,));self.last=state
  if self.age>240 and self.age%30==0 and os.environ.get('TWW_DEPARTURE_EXPECT_BLOCKER')=='1':
   for i in range(int(events['mHeaderP']['staffNum'])):
    row=events['mStaffP'][i]
    if row['mName'].string()!='PACKAGE' or int(row['mWipeDirection'])!=-1:continue
    after=int(gdb.parse_and_eval('g_dComIfG_gameInfo.save.mSavedata.mEvent.mFlags[0x24]'))&1
    assert not after,'failed departure incorrectly granted EventFlag'
    cut=events['mCutP'][int(row['mCurCutIdx'])];flag=int(cut['mFlagIdx'])
    flags=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtManager.mFlag.mFlag')
    assert not(int(flags[flag//32])&(1<<(flag%32))),'failed package cut marked done'
    assert state[3]==115 and demo,'departure event did not remain pending'
    print('[departure] EXPECTED BLOCKER VERIFIED: departure starts, package pending, EventFlag unchanged; cinematic/embarkation NOT completed')
    gdb.set_convenience_variable('tww_probe_success',1);return True
  if self.age%120==0:
   print('[departure] age=%d pos=%s ready=%s/%s/%s'%(self.age,tuple(float(p['current']['pos'][a]) for a in ('x','y','z')),cam['m100'],cam['m101'],cam['m102']))
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
