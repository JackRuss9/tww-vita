# Observer for a continuous, user-played Outset run from the title menu to the
# pirate ship. It never writes game memory or input: it logs stage changes,
# events, equipment and Mesa/Abe conversations, exports the actual savedata at
# each milestone (<probe dir>/checkpoints) and states when the run reaches
# A_umikz with control after the departure cinematic.
set pagination off
set confirm off
set print static-members off
set python print-stack full
handle SIGUSR1 SIGUSR2 SIGPIPE nostop noprint
python
import gdb,os,time
out=os.path.join(os.environ.get('SMS_GX_DUMP_DIR','.'),'checkpoints');os.makedirs(out,exist_ok=True)
start=time.time()
def log(msg):print('[island] t=%6.0fs %s'%(time.time()-start,msg),flush=True)
def export(tag):
 saved=gdb.parse_and_eval('g_dComIfG_gameInfo.save.mSavedata')
 path=os.path.join(out,'%s.bin'%tag)
 with open(path,'wb') as s:s.write(bytes(gdb.selected_inferior().read_memory(int(saved.address),saved.type.sizeof)))
 log('savedata exported: %s'%path)
class Island(gdb.Breakpoint):
 def __init__(self):
  super().__init__('daPy_lk_c::execute',internal=True)
  self.age=0;self.stage=None;self.event=None;self.equip=None;self.n=0;self.departed=False;self.calm=0;self.done=False
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
   self.event=event
   log('EVENT %d %s'%(event,name) if event>=0 else 'EVENT none (control)')
   if name=='departure_DEMO':self.departed=True
  st=gdb.parse_and_eval('g_dComIfG_gameInfo.save.mSavedata.mPlayer.mPlayerStatusA')
  equip=(int(st['mSelectEquip'][0]),int(st['mSelectEquip'][1]),int(st['mMaxLife']),int(st['mLife']))
  if equip!=self.equip:
   if self.equip is not None and equip[:3]!=self.equip[:3]:
    self.n+=1;export('%02d-equip-%02x-%02x'%(self.n,equip[0],equip[1]))
   if self.equip is None or equip[:3]!=self.equip[:3]:
    log('EQUIP sword=0x%02x shield=0x%02x hearts=%d/%d'%(equip[0],equip[1],equip[3],equip[2]))
   self.equip=equip
  demo=int(p['mDemo']['mDemoType'])
  if self.departed and not self.done and stage[0]=='A_umikz' and event<0 and not demo:
   self.calm+=1
   if self.calm==4:
    flag=int(gdb.parse_and_eval('g_dComIfG_gameInfo.save.mSavedata.mEvent.mFlags[0x24]'))&1
    log('FULL ISLAND RUN REACHED SHIP: departure watched in this run, EventFlag 0x2401=%d, A_umikz with control at %s'%(flag,pos))
    self.n+=1;export('%02d-ship-control'%self.n)
    self.done=True;gdb.set_convenience_variable('tww_probe_success',1)
  else:
   self.calm=0
  if self.age%1800==0:log('alive stage=%s pos=%s proc=%s demo=%d'%(stage[0],pos,p['mCurProc'],demo))
  return False
class YmTalk(gdb.Breakpoint):
 def __init__(self):super().__init__('daNpc_Ym1_c::endEvent',internal=True)
 def stop(self):
  a=gdb.parse_and_eval('this')
  log('MESA/ABE talk ended: %s type=%d message=0x%x'%('Mesa (Ym1)' if int(a['mSubType'])==1 else 'Abe (Ym2)',int(a['mType']),int(a['mCurrMsgNo'])))
  return False
class YmCreate(gdb.Breakpoint):
 def __init__(self):super().__init__('daNpc_Ym1_c::createInit',internal=True)
 def stop(self):
  a=gdb.parse_and_eval('this')
  log('MESA/ABE created: %s type=%d stage=%s'%('Mesa (Ym1)' if int(a['mSubType'])==1 else 'Abe (Ym2)',int(a['mType']),gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCurStage.mName').string()))
  return False
Island();YmTalk();YmCreate()
log('observer ready; checkpoints in %s; all input belongs to the player'%out)
end
source ../tools/crash.gdb
