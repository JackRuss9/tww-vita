# Passive observer for user-played sessions. Never writes game memory or input.
# Logs stage changes, events and equipment, exports the actual savedata at
# every stage change (<probe dir>/checkpoints) and, when one event stays active
# for 20 seconds, prints its staff/cut state once (who has not finished).
#
#   ./tools/probe-pc.sh --save-dir <card> --seconds 10800 --gdb "$PWD/tools/play_observer.gdb"
set pagination off
set confirm off
set print static-members off
set python print-stack full
handle SIGUSR1 SIGUSR2 SIGPIPE nostop noprint
python
import gdb,os,sys,time
sys.dont_write_bytecode=True
sys.path.insert(0,'../tools')
out=os.path.join(os.environ.get('SMS_GX_DUMP_DIR','.'),'checkpoints');os.makedirs(out,exist_ok=True)
start=time.time()
def log(msg):print('[play] t=%6.0fs %s'%(time.time()-start,msg),flush=True)
def export(tag):
 saved=gdb.parse_and_eval('g_dComIfG_gameInfo.save.mSavedata')
 path=os.path.join(out,'%s.bin'%tag)
 with open(path,'wb') as s:s.write(bytes(gdb.selected_inferior().read_memory(int(saved.address),saved.type.sizeof)))
 log('savedata exported: %s'%path)
def describe_event():
 events=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtManager.mList')
 header=events['mHeaderP']
 flags=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtManager.mFlag.mFlag')
 active=False
 def on(v):return 0<=v<10240 and bool(int(flags[v//32])&(1<<(v%32)))
 for i in range(int(header['eventNum'])):
  ev=events['mEventP'][i]
  if int(ev['mEventState'])!=2:continue
  active=True
  log('STALL active event %d %s'%(i,ev['mName'].string()))
  for j in range(int(ev['mNStaff'])):
   row=events['mStaffP'][int(ev['mStaffIdx'][j])];ci=int(row['mCurCutIdx'])
   if ci<0:
    log('STALL  staff %s no cut'%row['mName'].string());continue
   cut=events['mCutP'][ci];starts=[int(cut['mStartFlag'][k]) for k in range(3)]
   log('STALL  staff %s cut %s start=%s/%s done=%s hasAction=%s'%(row['mName'].string(),cut['mName'].string(),starts,[on(s) for s in starts],on(int(cut['mFlagIdx'])),row['mbHasAction']))
 if not active:return
 cam=gdb.parse_and_eval('((camera_process_class*)g_dComIfG_gameInfo.play.mCameraInfo[0].mpCamera)->mCamera')
 log('STALL  camera ready=%s/%s/%s frame=%s'%(cam['m100'],cam['m101'],cam['m102'],cam['m11C']))
class Play(gdb.Breakpoint):
 def __init__(self):
  super().__init__('daPy_lk_c::execute',internal=True)
  self.age=0;self.stage=None;self.event=None;self.equip=None;self.n=0;self.since=0;self.dumped=False
 def stop(self):
  self.age+=1
  if self.age%15:return False
  try:p=gdb.parse_and_eval('this')
  except gdb.error:p=gdb.parse_and_eval('(daPy_lk_c*)i_this')
  play=gdb.parse_and_eval('g_dComIfG_gameInfo.play')
  stage=(play['mCurStage']['mName'].string(),int(play['mCurStage']['mRoomNo']),int(play['mCurStage']['mPoint']),int(play['mCurStage']['mLayer']))
  pos=tuple(round(float(p['current']['pos'][a]),1) for a in ('x','y','z'))
  if stage!=self.stage:
   self.stage=stage;self.n+=1
   log('STAGE %s room %d point %d layer %d at %s'%(stage+(pos,)))
   export('%02d-%s'%(self.n,stage[0]))
  event=int(play['mEvtCtrl']['mEventId']);name=''
  if event>=0:
   try:name=play['mEvtManager']['mList']['mEventP'][event]['mName'].string()
   except gdb.error:pass
  if event!=self.event:
   self.event=event;self.since=time.time();self.dumped=False
   log('EVENT %d %s'%(event,name) if event>=0 else 'EVENT none (control)')
  elif event>=0 and not self.dumped and time.time()-self.since>20:
   self.dumped=True
   try:describe_event()
   except gdb.error as e:log('STALL describe failed: %s'%e)
  st=play['mPlayerInfo'][0]['mpPlayer']
  sv=gdb.parse_and_eval('g_dComIfG_gameInfo.save.mSavedata.mPlayer.mPlayerStatusA')
  equip=(int(sv['mSelectEquip'][0]),int(sv['mSelectEquip'][1]),int(sv['mMaxLife']))
  if equip!=self.equip:
   self.equip=equip;log('EQUIP sword=0x%02x shield=0x%02x maxLife=%d'%equip)
  if self.age%1800==0:log('alive stage=%s pos=%s proc=%s demo=%d'%(stage[0],pos,p['mCurProc'],int(p['mDemo']['mDemoType'])))
  return False
Play()
log('observer ready; checkpoints in %s; all input belongs to the player'%out)
end
source ../tools/crash.gdb
