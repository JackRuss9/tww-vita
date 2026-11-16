# Isolated Outset story-camera regression, NOT a complete normal playthrough.
# TWW_TEST_TELESCOPE_MODEL=1 additionally equips the earned telescope and
# checks its model/opening view, not completion of Aryll's guided tutorial.
# Earns Hero's Clothes in LinkRM, requests sea point 6 and walks to Aryll.
# Default driver injects next-stage requests and keyboard input; the optional
# model check also assigns the earned item to X. Story flags are earned by
# actual events. Checks framing and control return.
# ./tools/probe-pc.sh --outset --headless --deterministic --seconds 600 --gdb "$PWD/tools/aryll_story.gdb"
set pagination off
set confirm off
handle SIGUSR1 SIGUSR2 SIGPIPE nostop noprint
python
import gdb
import struct
import math
import os
test_telescope_model=os.environ.get("TWW_TEST_TELESCOPE_MODEL")=="1"
hero_done = False
npc_xyz = None
npc_address = None
saw_telescope = False
particle_emitted = False
selectors = 0
keybase = int(gdb.parse_and_eval("&'(anonymous namespace)::g_key'"))
class RoomTransition(gdb.Breakpoint):
    def __init__(self):
        super().__init__('daPy_lk_c::execute')
        self.outside, self.inside, self.origin, self.requested = 0, 0, None, False
        self.seen_demo, self.free_since = False, None
    def stop(self):
        global hero_done
        obj = gdb.parse_and_eval('this')
        stage = gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCurStage.mName').string()
        if stage == 'sea' and not self.requested:
            self.outside += 1
            if self.outside == 180:
                self.requested = True
                print('[room] diagnostic scene request: LinkRM point 200 room 0 layer 8 wipe 0')
                request = int(gdb.parse_and_eval('&g_dComIfG_gameInfo.play.mNextStage'))
                gdb.selected_inferior().write_memory(request, struct.pack('<8shbbbb', b'LinkRM', 200, 0, 8, 1, 0))
        elif stage == 'LinkRM':
            self.inside += 1
            xyz = tuple(float(obj['current']['pos'][axis]) for axis in ('x','y','z'))
            demo = int(obj['mDemo']['mDemoType'])
            self.seen_demo |= demo != 0
            if self.inside % 120 == 1:
                print('[clothes] frame=%d demo=%d/%d casual=%d selectors=%d emitted=%s' %
                      (self.inside,demo,int(obj['mDemo']['mDemoMode']),
                       int(obj['mNoResetFlg1']) & 8, selectors, particle_emitted))
            if self.seen_demo and demo == 0 and particle_emitted:
                gdb.selected_inferior().write_memory(keybase + 44, b'\0')
                if self.free_since is None: self.free_since = self.inside
                age = self.inside - self.free_since
                if age == 60: self.origin = xyz
                gdb.selected_inferior().write_memory(keybase + 26, bytes([60 <= age < 90]))
                if age >= 100 and self.origin:
                    distance = sum((xyz[i]-self.origin[i])**2 for i in (0,2))**0.5
                    print('[clothes] post-scene movement=%.3f casual=%d' % (distance,int(obj['mNoResetFlg1']) & 8))
                    if distance > 10 and not int(obj['mNoResetFlg1']) & 8:
                        if not hero_done:
                            print('[aryll] clothes scene/control passed; diagnostic return to sea point 6, keeping earned story flags')
                            hero_done = True
                            request = int(gdb.parse_and_eval('&g_dComIfG_gameInfo.play.mNextStage'))
                            gdb.selected_inferior().write_memory(request, struct.pack('<8shbbbb', b'sea', 6, 44, 0, 1, 0))
                        return False
            else:
                gdb.selected_inferior().write_memory(keybase + 44, bytes([91 <= self.inside % 120 < 120]))
        return False
class SelectorReturn(gdb.FinishBreakpoint):
    def __init__(self, address, expected):
        super().__init__(gdb.newest_frame(), internal=True)
        self.address, self.expected = address, expected
    def stop(self):
        global selectors
        actual = int(gdb.parse_and_eval('((JStudio_JParticle::TAdaptor_particle*)0x%x)->field_0x1B0' % self.address)) & 0xffffffff
        print('[clothes] particle selector actual=0x%x expected=0x%x' % (actual,self.expected))
        assert actual == self.expected
        selectors += 1
        return False
class Selector(gdb.Breakpoint):
    def stop(self):
        raw = bytes(gdb.selected_inferior().read_memory(int(gdb.parse_and_eval('r5')),4))
        SelectorReturn(int(gdb.parse_and_eval('this')),int.from_bytes(raw,'big'))
        return False
class Emit(gdb.Breakpoint):
    def stop(self):
        global particle_emitted
        group, resource, effect = [int(gdb.parse_and_eval(x)) for x in ('groupID','rmID','userID')]
        if effect == 506: print('[clothes] hero presentation particle group=%d resource=%d effect=%d' % (group,resource,effect))
        assert group < 16 and resource < 8
        if selectors: particle_emitted = True
        return False
selector_address = int(gdb.parse_and_eval("&'JStudio_JParticle::TAdaptor_particle::adaptor_do_PARTICLE(JStudio::data::TEOperationData, void const*, unsigned long)'"))
Selector('*0x%x' % selector_address)
Emit('JPAEmitterManager::createSimpleEmitterID')
class Aryll(gdb.Breakpoint):
    def __init__(self):
        super().__init__('daNpc_Ls1_c::_execute')
        self.last = None
    def stop(self):
        global npc_xyz, npc_address
        if not hero_done: return False
        obj=gdb.parse_and_eval('this')
        if int(obj['mType']) != 0: return False
        npc_address=int(obj)
        npc_xyz=tuple(float(obj['current']['pos'][x]) for x in ('x','y','z'))
        state=tuple(int(obj[x]) for x in ('mType','m851','m850','mEventIndex'))
        if state!=self.last:
            self.last=state
            print('[aryll] state type/wait/order/event=%s pos=%s' % (state,npc_xyz))
        return False
class Event(gdb.Breakpoint):
    def stop(self):
        if not hero_done:return False
        obj=gdb.parse_and_eval('this')
        idx=int(gdb.parse_and_eval('eventIdx'))
        if idx>=0:
            name=obj['mList']['mEventP'][idx]['mName'].string()
            print('[aryll] event %d %s' % (idx,name))
        return False
class Navigate(gdb.Breakpoint):
    def __init__(self):
        super().__init__('daPy_lk_c::execute')
        self.n,self.last,self.origin,self.free_since=0,None,None,None
        self.scope_since,self.scope_seen=None,False
    def press(self,key):
        for name,code in {'up':26,'down':22,'left':4,'right':7,'a':44}.items():
            gdb.selected_inferior().write_memory(keybase+code,bytes([key==name]))
    def scope_model(self,obj):
        if self.scope_since is None:
            assert int(gdb.parse_and_eval('g_dComIfG_gameInfo.save.mSavedata.mPlayer.mPlayerItem.mItems[0]'))==0x20
            for i in (0,1):
                size=int(obj['mpItemHeaps'][i]['mSize'])
                print('[scope-model] equipment heap %d capacity=0x%x'%(i,size))
                assert size>=0xe000
            # Assign X to the inventory slot earned by the event, without
            # testing the inventory assignment UI or modifying quest flags.
            for address,value in [('&g_dComIfG_gameInfo.save.mSavedata.mPlayer.mPlayerStatusA.mSelectItem[0]',0),('&g_dComIfG_gameInfo.play.mSelectItem[0]',0x20)]:
                gdb.selected_inferior().write_memory(int(gdb.parse_and_eval(address)),bytes([value]))
            self.scope_since=self.n
        age=self.n-self.scope_since
        self.press(None)
        gdb.selected_inferior().write_memory(keybase+25,bytes([5<=age<20]))
        if int(obj['mCurProc'])==0:
            assert int(obj['mpEquipItemModel'])!=0
            self.scope_seen=True
            cam=gdb.parse_and_eval('((camera_process_class*)g_dComIfG_gameInfo.play.mCameraInfo[0].mpCamera)->mCamera')
            assert all(math.isfinite(float(cam[f][x])) for f in ('mEye','mCenter') for x in ('x','y','z'))
            assert math.isfinite(float(cam['mFovy'])) and 0<float(cam['mFovy'])<180
        if age>=80 and self.scope_seen:
            print('[scope-model] SUCCESS: earned telescope, model allocation and finite opening view; guided tutorial not validated')
            gdb.set_convenience_variable('tww_probe_success',1)
            return True
        return False
    def stop(self):
        global saw_telescope
        if not hero_done:return False
        if gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCurStage.mName').string()!='sea':return False
        self.n+=1
        # Rotate the live manual camera before approaching Aryll, so this
        # regression also checks manual -> event/TALK -> gameplay handover.
        gdb.selected_inferior().write_memory(keybase+15,bytes([self.n<=45]))
        if self.n%5:return False
        obj=gdb.parse_and_eval('this')
        xyz=tuple(float(obj['current']['pos'][x]) for x in ('x','y','z'))
        demo=int(obj['mDemo']['mDemoType'])
        inventory=int(gdb.parse_and_eval('g_dComIfG_gameInfo.save.mSavedata.mPlayer.mPlayerItem.mItems[0]'))
        saw_telescope |= inventory==0x20
        if self.n%120==0:print('[aryll] player=%s demo=%d/%d telescope-slot=0x%x' % (xyz,demo,int(obj['mDemo']['mDemoMode']),inventory))
        if saw_telescope and demo==0:
            if self.free_since is None:self.free_since=self.n
            age=self.n-self.free_since
            if age==60:self.origin=xyz
            self.press('up' if 60<=age<90 else None)
            if age>=100 and self.origin:
                distance=sum((xyz[i]-self.origin[i])**2 for i in (0,2))**0.5
                if not test_telescope_model or age==100:print('[aryll] post-telescope movement=%.3f' % distance)
                if distance>10:
                    if test_telescope_model:return self.scope_model(obj)
                    print('[aryll] SUCCESS: earned hero clothes, automatic Aryll scenes, telescope inventory and control return')
                    gdb.set_convenience_variable('tww_probe_success',1)
                    return True
        elif demo!=0:
            self.press('a' if 91<=self.n%120<120 else None)
        elif npc_xyz and self.n>=60:
            dx,dz=npc_xyz[0]-xyz[0],npc_xyz[2]-xyz[2]
            distance=(dx*dx+dz*dz)**0.5
            if distance>90:
                view=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCameraInfo[0].mpCamera->view.mLookat')
                fx=float(view['mCenter']['x'])-float(view['mEye']['x'])
                fz=float(view['mCenter']['z'])-float(view['mEye']['z'])
                forward,right=dx*fx+dz*fz,-dx*fz+dz*fx
                self.press(('up' if forward>0 else 'down') if abs(forward)>abs(right) else ('right' if right>0 else 'left'))
            else:self.press(None)
        return False
class RelativeReturn(gdb.FinishBreakpoint):
    def __init__(self,address,expected,cut):
        super().__init__(gdb.newest_frame(),internal=True)
        self.address,self.expected,self.cut=address,expected,cut
    def stop(self):
        cam=gdb.parse_and_eval('((dCamera_c*)0x%x)' % self.address)
        for field, expected in self.expected.items():
            actual=tuple(float(cam['mViewCache'][field][x]) for x in ('x','y','z'))
            error=max(abs(actual[i]-expected[i]) for i in range(3))
            print('[camera-check] cut=%d %s actual=%s expected=%s max-error=%.5f' % (self.cut,field,actual,expected,error))
            assert error<0.15
        return False
class Relative(gdb.Breakpoint):
    def stop(self):
        if not hero_done or npc_address is None:return False
        sp=int(gdb.parse_and_eval('$esp'))
        address=struct.unpack('<I',bytes(gdb.selected_inferior().read_memory(sp+4,4)))[0]
        cam=gdb.parse_and_eval('((dCamera_c*)0x%x)' % address)
        if int(cam['m11C'])!=0:return False
        staff=int(cam['mEventData']['mStaffIdx'])
        events=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtManager.mList')
        cut=int(events['mStaffP'][staff]['mCurCutIdx'])
        di=int(events['mCutP'][cut]['mFirstDataIdx']);values={}
        while di>=0:
            d=events['mDataP'][di];name=d['mName'].string();typ=int(d['mSubstanceType']);offset=int(d['mSubstanceIdx'])
            if typ==1:values[name]=tuple(float(events['mFDataP'][offset+i]) for i in range(3))
            elif typ==4:values[name]=(events['mSDataP']+offset).string()
            di=int(d['mNextIdx'])
        if values.get('RelActor')!='@STARTER':return False
        actor=gdb.parse_and_eval('((daNpc_Ls1_c*)0x%x)' % npc_address)
        anchor=tuple(float(actor['attention_info']['position'][x]) for x in ('x','y','z'))
        angle=int(actor['shape_angle']['y'])*math.pi/32768
        def transform(v):return (anchor[0]+v[0]*math.cos(angle)+v[2]*math.sin(angle),anchor[1]+v[1],anchor[2]-v[0]*math.sin(angle)+v[2]*math.cos(angle))
        RelativeReturn(address,{'mEye':transform(values['Eye']),'mCenter':transform(values['Center'])},cut)
        return False
class GetItemReturn(gdb.FinishBreakpoint):
    def __init__(self,address):
        super().__init__(gdb.newest_frame(),internal=True)
        self.address=address
    def stop(self):
        cam=gdb.parse_and_eval('((dCamera_c*)0x%x)' % self.address)
        address=int(gdb.parse_and_eval('&((dCamera_c*)0x%x)->mWork' % self.address))
        state=struct.unpack('<5ifhh',bytes(gdb.selected_inferior().read_memory(address,28)))
        if state[0]!=GetItemTrace.last:
            GetItemTrace.last=state[0]
            print('[getitem-check] step/elapsed/limit/wait/move/radius/pitch/yaw=%s ready=%d/%d/%d' % (state,int(cam['m100']),int(cam['m101']),int(cam['m102'])))
        assert all(math.isfinite(float(cam['mViewCache'][f][x])) for f in ('mEye','mCenter') for x in ('x','y','z'))
        if state[0]==99:assert int(cam['m100']) and int(cam['m101']) and int(cam['m102'])
        return False
class GetItemTrace(gdb.Breakpoint):
    last=None
    def stop(self):
        if not hero_done:return False
        sp=int(gdb.parse_and_eval('$esp'))
        address=struct.unpack('<I',bytes(gdb.selected_inferior().read_memory(sp+4,4)))[0]
        GetItemReturn(address)
        return False
class CameraCuts(gdb.Breakpoint):
    def __init__(self):
        super().__init__('dCamera_c::eventCamera')
        self.last=None
    def stop(self):
        if not hero_done:return False
        cam=gdb.parse_and_eval('this');staff=int(cam['mEventData']['mStaffIdx'])
        if staff<0:return False
        events=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtManager.mList')
        cut=int(events['mStaffP'][staff]['mCurCutIdx'])
        if cut!=self.last:
            self.last=cut
            print('[aryll] camera cut=%d %s ready=%d/%d/%d' % (cut,events['mCutP'][cut]['mName'].string(),int(cam['m100']),int(cam['m101']),int(cam['m102'])))
        return False
class TalkReturn(gdb.FinishBreakpoint):
    def __init__(self,address):
        super().__init__(gdb.newest_frame(),internal=True)
        self.address=address
    def stop(self):
        cam=gdb.parse_and_eval('((dCamera_c*)0x%x)' % self.address)
        address=int(gdb.parse_and_eval('&((dCamera_c*)0x%x)->mWork' % self.address))
        state=struct.unpack('<Iiiffffffhh',bytes(gdb.selected_inferior().read_memory(address,40)))
        elapsed=state[2]
        assert all(math.isfinite(float(cam['mViewCache'][f][x])) for f in ('mEye','mCenter') for x in ('x','y','z'))
        if elapsed < 20:
            assert not int(cam['m100']) and not int(cam['m101']) and not int(cam['m102'])
        elif elapsed == 20:
            assert int(cam['m100']) and int(cam['m101']) and int(cam['m102'])
            actual=tuple(float(cam['mViewCache']['mCenter'][x]) for x in ('x','y','z'))
            assert max(abs(actual[i]-state[6+i]) for i in range(3)) < 0.1
            assert abs(float(cam['mViewCache']['mFovy'])-state[5]) < 0.01
            print('[talk-check] real framing complete at frame 20, finite view, target center/FOV reached, ready=1/1/1')
        return False
class TalkTrace(gdb.Breakpoint):
    def stop(self):
        if not hero_done:return False
        sp=int(gdb.parse_and_eval('$esp'))
        address=struct.unpack('<I',bytes(gdb.selected_inferior().read_memory(sp+4,4)))[0]
        TalkReturn(address)
        return False
address=int(gdb.parse_and_eval("&'dCamera_c::talktoCamera(long)'"))
TalkTrace('*0x%x' % address)
address=int(gdb.parse_and_eval("&'dCamera_c::fixedFrameEvCamera()'"))
Relative('*0x%x' % address)
CameraCuts()
address=int(gdb.parse_and_eval("&'dCamera_c::getItemEvCamera()'"))
GetItemTrace('*0x%x' % address)
Aryll()
Event('dEvent_manager_c::order')
Navigate()
RoomTransition()
end
source ../tools/crash.gdb
