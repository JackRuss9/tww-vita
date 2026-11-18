"""Input-only diagnostic continuation into Orca's house and sword training.

The parent driver supplies earned savedata/stage requests; this module never
changes quest flags, hit counters, inventory or camera readiness.
"""
import gdb
import os
import struct
import math

class OrcaActor(gdb.Breakpoint):
    def __init__(self,owner):
        super().__init__('daNpc_Ji1_c::_execute',internal=True)
        self.owner=owner
    def stop(self):
        actor=gdb.parse_and_eval('this')
        self.owner.npc=int(actor)
        self.owner.actors[int(actor['mBsPcId'])]=int(actor)
        return False

class DoorActor(gdb.Breakpoint):
    def __init__(self,owner):
        super().__init__('daKnob00_Execute',internal=True)
        self.owner=owner
    def stop(self):
        actor=gdb.parse_and_eval('i_this')
        self.owner.actors[int(actor['mBsPcId'])]=int(actor)
        return False

class ItemCreated(gdb.FinishBreakpoint):
    def __init__(self,owner,address):
        super().__init__(gdb.newest_frame(),internal=True)
        self.owner,self.address=owner,address
    def stop(self):
        actor=gdb.parse_and_eval('((daNpc_Ji1_c*)0x%x)'%self.address)
        if int(actor['mCreateItemNo'])==0x38:
            self.owner.present=True
            print('[orca] real sword presentation item created')
        return False

class Present(gdb.Breakpoint):
    def __init__(self,owner):
        super().__init__('daNpc_Ji1_c::createItem',internal=True)
        self.owner=owner
    def stop(self):
        ItemCreated(self.owner,int(gdb.parse_and_eval('this')))
        return False

class PartnerFrameDone(gdb.FinishBreakpoint):
    def __init__(self,cam,expected):
        super().__init__(gdb.newest_frame(),internal=True)
        self.cam,self.expected=cam,expected
    def stop(self):
        camera=gdb.parse_and_eval('((dCamera_c*)0x%x)'%self.cam)
        for field,expected in self.expected.items():
            actual=[float(camera['mViewCache'][field][v]) for v in ('x','y','z')]
            error=max(abs(actual[i]-expected[i]) for i in range(3))
            assert error<.2,(field,error)
        assert all(int(camera[f]) for f in ('m100','m101','m102'))
        print('[orca-camera] PARTNER/oo transformed Eye/Center and ready verified')
        return False

class PartnerFrame(gdb.Breakpoint):
    def __init__(self,owner):
        super().__init__('dCamera_c::fixedFrameEvCamera',internal=True)
        self.owner=owner
    def stop(self):
        camera=gdb.parse_and_eval('this')
        if int(camera['m11C'])!=0:return False
        stage=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCurStage.mName').string()
        if stage!='Ojhous':return False
        staff=int(camera['mEventData']['mStaffIdx'])
        if staff<0:return False
        events=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtManager.mList')
        cut=int(events['mStaffP'][staff]['mCurCutIdx'])
        di=int(events['mCutP'][cut]['mFirstDataIdx']);values={}
        while 0<=di<int(events['mHeaderP']['dataNum']):
            data=events['mDataP'][di]
            name=data['mName'].string();kind=int(data['mSubstanceType']);off=int(data['mSubstanceIdx'])
            if kind==1:values[name]=tuple(float(events['mFDataP'][off+i]) for i in range(3))
            elif kind==4:values[name]=(events['mSDataP']+off).string()
            di=int(data['mNextIdx'])
        if values.get('RelActor')!='@PARTNER':return False
        pid=int(gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtCtrl.mPt2'))
        player=camera['mpPlayerActor']
        if int(player['mBsPcId'])==pid:actor=player
        elif pid in self.owner.actors:
            actor=gdb.parse_and_eval('((fopAc_ac_c*)0x%x)'%self.owner.actors[pid])
        else:
            print('[orca-camera] participant %d not cached yet; numeric observation deferred'%pid)
            return False
        anchor=[float(actor['attention_info']['position'][v]) for v in ('x','y','z')]
        angle=int(actor['shape_angle']['y'])*math.pi/32768
        def transform(v):return (anchor[0]+v[0]*math.cos(angle)+v[2]*math.sin(angle),anchor[1]+v[1],anchor[2]-v[0]*math.sin(angle)+v[2]*math.cos(angle))
        PartnerFrameDone(int(camera),{'mEye':transform(values['Eye']),'mCenter':transform(values['Center'])})
        return False

class UniformDone(gdb.FinishBreakpoint):
    completed_timers=set()
    def __init__(self,camera,frame,values):
        super().__init__(gdb.newest_frame(),internal=True)
        self.camera,self.frame,self.values=camera,frame,values
    def stop(self):
        camera=gdb.parse_and_eval('((dCamera_c*)0x%x)'%self.camera)
        timer=self.values['Timer'][0]
        ratio=min(1,(self.frame+1)/timer) if timer>0 else 1
        for field,start,end in [('mEye','StartEye','Eye'),('mCenter','StartCenter','Center')]:
            expected=[self.values[start][i]+ratio*(self.values[end][i]-self.values[start][i]) for i in range(3)]
            actual=[float(camera['mViewCache'][field][v]) for v in ('x','y','z')]
            assert max(abs(actual[i]-expected[i]) for i in range(3))<.002,(field,self.frame,actual,expected)
        fovy=self.values['StartFovy'][0]+ratio*(self.values['Fovy'][0]-self.values['StartFovy'][0])
        assert abs(float(camera['mViewCache']['mFovy'])-fovy)<.001
        assert all(int(camera[f]) for f in ('m100','m101','m102'))
        assert bool(self.return_value)==(self.frame>=timer),(self.frame,self.return_value)
        if bool(self.return_value):UniformDone.completed_timers.add(timer)
        if self.frame in (0,timer//2-1,timer-1,timer):
            print('[orca-camera] UNITRANS frame=%d/%d ratio=%.6f Eye/Center/FOV/ready/end verified'%(self.frame,timer,ratio))
        return False

class UniformFrame(gdb.Breakpoint):
    def __init__(self):
        super().__init__('dCamera_c::uniformTransEvCamera',internal=True)
    def stop(self):
        camera=gdb.parse_and_eval('this')
        if gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCurStage.mName').string()!='Ojhous':return False
        events=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtManager.mList')
        staff=int(camera['mEventData']['mStaffIdx'])
        if staff<0:return False
        cut=int(events['mStaffP'][staff]['mCurCutIdx'])
        di=int(events['mCutP'][cut]['mFirstDataIdx']);values={}
        while 0<=di<int(events['mHeaderP']['dataNum']):
            data=events['mDataP'][di];kind=int(data['mSubstanceType']);off=int(data['mSubstanceIdx']);count=int(data['mSubstanceSize'])
            if kind in (0,1):values[data['mName'].string()]=[float(events['mFDataP'][off+i]) for i in range(count*(3 if kind==1 else 1))]
            elif kind==3:values[data['mName'].string()]=[int(events['mIDataP'][off+i]) for i in range(count)]
            elif kind==4:values[data['mName'].string()]=(events['mSDataP']+off).string()
            di=int(data['mNextIdx'])
        if values.get('BSpCurve')==[0] and values.get('BGCheck')==[0] and 'RelActor' not in values:
            UniformDone(int(camera),int(camera['m11C']),values)
        return False

class FixedPositionDone(gdb.FinishBreakpoint):
    def __init__(self,camera,eye,center):
        super().__init__(gdb.newest_frame(),internal=True)
        self.camera,self.eye,self.center=camera,eye,center
    def stop(self):
        camera=gdb.parse_and_eval('((dCamera_c*)0x%x)'%self.camera)
        for field,expected in [('mEye',self.eye),('mCenter',self.center)]:
            actual=[float(camera['mViewCache'][field][v]) for v in ('x','y','z')]
            assert max(abs(actual[i]-expected[i]) for i in range(3))<.2,(field,actual,expected)
        assert all(int(camera[f]) for f in ('m100','m101','m102'))
        assert bool(self.return_value)
        assert 0<float(camera['mViewCache']['mFovy'])<180
        print('[orca-camera] FIXEDPOS player-relative Eye, smoothed Center, readiness/end verified')
        return False

class FixedPositionFrame(gdb.Breakpoint):
    def __init__(self):
        super().__init__('dCamera_c::fixedPositionEvCamera',internal=True)
    def stop(self):
        camera=gdb.parse_and_eval('this')
        if int(camera['m11C'])!=0:return False
        if gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCurStage.mName').string()!='Ojhous':return False
        events=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtManager.mList')
        staff=int(camera['mEventData']['mStaffIdx'])
        if staff<0:return False
        cut=int(events['mStaffP'][staff]['mCurCutIdx'])
        di=int(events['mCutP'][cut]['mFirstDataIdx']);values={}
        while 0<=di<int(events['mHeaderP']['dataNum']):
            data=events['mDataP'][di];kind=int(data['mSubstanceType']);off=int(data['mSubstanceIdx']);count=int(data['mSubstanceSize'])
            if kind in (0,1):values[data['mName'].string()]=[float(events['mFDataP'][off+i]) for i in range(count*(3 if kind==1 else 1))]
            elif kind==3:values[data['mName'].string()]=[int(events['mIDataP'][off+i]) for i in range(count)]
            elif kind==4:values[data['mName'].string()]=(events['mSDataP']+off).string()
            di=int(data['mNextIdx'])
        if values.get('RelActor')!='@PLAYER' or 'Timer' in values:return False
        if values.get('Target','@PLAYER')!='@PLAYER' or values.get('CtrGap',[0,0,0])!=[0,0,0]:return False
        player=camera['mpPlayerActor']
        anchor=[float(player['attention_info']['position'][v]) for v in ('x','y','z')]
        yaw=int(player['shape_angle']['y'])*math.pi/32768
        offset=values['Eye']
        eye=[anchor[0]+offset[0]*math.cos(yaw)+offset[2]*math.sin(yaw),anchor[1]+offset[1],anchor[2]-offset[0]*math.sin(yaw)+offset[2]*math.cos(yaw)]
        previous=[float(camera['mViewCache']['mCenter'][v]) for v in ('x','y','z')]
        cushion=values.get('CtrCus',[1])[0]
        center=[previous[i]+cushion*(anchor[i]-previous[i]) for i in range(3)]
        FixedPositionDone(int(camera),eye,center)
        return False

class OrcaStory(gdb.Breakpoint):
    def __init__(self,keybase):
        super().__init__('daPy_lk_c::execute',internal=True)
        self.keys=keybase
        self.axes=int(gdb.parse_and_eval("&'(anonymous namespace)::g_axis'"))
        self.npc=None
        self.actors={}
        self.age=0
        self.last=None
        self.phase_age=0
        self.last_phase=None
        self.ready_age=0
        self.last_cut=None
        self.prep_age=0
        self.phases=set()
        self.present=False
        UniformDone.completed_timers.clear()
        self.free_age=0
        OrcaActor(self)
        DoorActor(self)
        Present(self)
        PartnerFrame(self)
        UniformFrame()
        FixedPositionFrame()
        # The inherited STARTER observer assumes Aryll's actor. It has
        # already checked her cuts; use this stage-specific observer now.
        for bp in gdb.breakpoints():
            if type(bp).__name__=='Relative':bp.enabled=False
        request=int(gdb.parse_and_eval('&g_dComIfG_gameInfo.play.mNextStage'))
        gdb.selected_inferior().write_memory(request,struct.pack('<8shbbbb',b'Ojhous',0,0,-1,1,0))
        print('[orca] diagnostic request Ojhous point 0; preserve earned story state')
    def key(self,code,on):
        gdb.selected_inferior().write_memory(self.keys+code,bytes([bool(on)]))
    def stop(self):
        if gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCurStage.mName').string()!='Ojhous':return False
        self.age+=1
        if not self.npc:return False
        player=gdb.parse_and_eval('this')
        actor=gdb.parse_and_eval('((daNpc_Ji1_c*)0x%x)'%self.npc)
        phase=int(actor['field_0xD70'])
        action=str(actor['mAction'])
        demo=int(player['mDemo']['mDemoType'])
        equip=int(gdb.parse_and_eval('g_dComIfG_gameInfo.save.mSavedata.mPlayer.mPlayerStatusA.mSelectEquip[0]'))
        state=(int(actor['mMsgNo']),phase,int(actor['field_0xC98']),demo,equip,action,int(actor['field_0xC78']),int(actor['mAnimation']))
        if state!=self.last:
            print('[orca] msg/phase/hits/demo/sword/action=%s'%(state,));self.last=state
        for code in (26,22,4,7,44,225,20,224):self.key(code,False)
        gdb.selected_inferior().write_memory(self.axes,struct.pack('<hh',0,0))
        if self.age%120==0:print('[orca-heartbeat] age=%d state=%s'%(self.age,state))
        if 'plmoveAction' in action:
            self.prep_age+=1
            if self.prep_age==120:describe()
        if demo:
            self.key(44,70<=self.age%120<95)
            return False
        xyz=[float(player['current']['pos'][v]) for v in ('x','y','z')]
        target=[float(actor['current']['pos'][v]) for v in ('x','y','z')]
        dx,dz=target[0]-xyz[0],target[2]-xyz[2]
        distance=(dx*dx+dz*dz)**.5
        cut_state=(int(player['mCurProc']),int(player['mCutType']))
        if cut_state!=self.last_cut:
            self.last_cut=cut_state
            print('[orca-input] phase=%d proc/cut=%s distance=%.1f'%(phase,cut_state,distance))
        if self.age%120==0:print('[orca] distance=%.1f proc=%s cut=%s'%(distance,player['mCurProc'],player['mCutType']))
        if os.environ.get('TWW_TEST_ORCA_CAMERAS')=='1' and phase==0 and 'teachAction' in action and {30,60}.issubset(UniformDone.completed_timers):
            print('[orca-camera] SUCCESS: both linear transitions and initial training control; sword training not tested in this mode')
            gdb.set_convenience_variable('tww_probe_success',1)
            return True
        if self.present and equip==0x38 and 'teachAction' not in action:
            self.free_age+=1
            if self.free_age==30:self.origin=xyz
            self.key(26,30<=self.free_age<60)
            if self.free_age>=80:
                distance=sum((xyz[i]-self.origin[i])**2 for i in (0,2))**.5
                assert {0,1,2,3,4,5}.issubset(self.phases),self.phases
                assert distance>10,distance
                print('[orca] SUCCESS: earned sword presentation/inventory, training phases and movement=%.3f'%distance)
                checkpoint=os.environ.get('TWW_SWORD_SAVE_CHECKPOINT')
                if checkpoint:
                    saved=gdb.parse_and_eval('g_dComIfG_gameInfo.save.mSavedata')
                    with open(checkpoint,'wb') as output:output.write(bytes(gdb.selected_inferior().read_memory(int(saved.address),saved.type.sizeof)))
                    print('[orca] retained genuinely earned sword savedata locally')
                if os.environ.get('TWW_TEST_FOREST')=='1':
                    from forest_story import ForestStory
                    ForestStory(self.keys)
                    self.free_age=-1000000
                    return False
                gdb.set_convenience_variable('tww_probe_success',1)
                return True
            return False
        if 'teachAction' in action:
            self.phases.add(phase)
            if phase!=self.last_phase:self.phase_age=0;self.last_phase=phase
            self.phase_age+=1
            self.key(20,phase in (1,2,4,5))
            if phase==2:self.key(26,True)
            elif phase<4 and distance>140:
                look=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCameraInfo[0].mpCamera->view.mLookat')
                fx=float(look['mCenter']['x'])-float(look['mEye']['x'])
                fz=float(look['mCenter']['z'])-float(look['mEye']['z'])
                forward,right=dx*fx+dz*fz,-dx*fz+dz*fx
                self.key((26 if forward>0 else 22) if abs(forward)>abs(right) else (7 if right>0 else 4),True)
            if phase<=2:
                if phase<2 and distance>140:self.ready_age=0
                else:self.ready_age+=1
                self.key(225,self.ready_age>10 and self.age%30<5)
            elif phase==3:self.key(225,self.phase_age%120<75)
            elif phase==4:
                status=int(gdb.parse_and_eval('g_dComIfG_gameInfo.play.mDoStatus'))
                self.key(44,status==0x1a)
            elif phase==5:self.key(44,self.phase_age%60<5)
        elif distance<150:
            self.key(44,70<=self.age%120<95)
        else:
            look=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCameraInfo[0].mpCamera->view.mLookat')
            fx=float(look['mCenter']['x'])-float(look['mEye']['x'])
            fz=float(look['mCenter']['z'])-float(look['mEye']['z'])
            forward,right=dx*fx+dz*fz,-dx*fz+dz*fx
            self.key((26 if forward>0 else 22) if abs(forward)>abs(right) else (7 if right>0 else 4),True)
        override=os.environ.get('TWW_ORCA_CONTROLLER')
        if override:
            namespace={'gdb':gdb,'struct':struct}
            exec(open(override).read(),namespace)
            namespace['control'](self,player,actor,distance,phase,action)
        return False


def describe():
    owners=[bp for bp in gdb.breakpoints() if type(bp).__name__=='OrcaStory']
    for owner in owners:
        if owner.npc and gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCurStage.mName').string()=='Ojhous':
            actor=gdb.parse_and_eval('((daNpc_Ji1_c*)0x%x)'%owner.npc)
            print('[orca-stop] action=%s state=%s animation=%s phase=%s eventIds=%s'%(actor['mAction'],actor['field_0xC78'],actor['mAnimation'],actor['field_0xD70'],actor['mEventIdx']))
    camera=gdb.parse_and_eval('((camera_process_class*)g_dComIfG_gameInfo.play.mCameraInfo[0].mpCamera)->mCamera')
    print('[orca-stop] camera ready=%s/%s/%s event=%s'%(camera['m100'],camera['m101'],camera['m102'],camera['mEventData']))
    events=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtManager.mList')
    header=events['mHeaderP']
    print('[orca-stop] header=%s'%header)
    # The header supplies array bounds; retain active staff and cuts only.
    print('[orca-stop] header layout=%s'%header.dereference())
    staff=int(camera['mEventData']['mStaffIdx'])
    if staff>=0:
        row=events['mStaffP'][staff];cut=events['mCutP'][int(row['mCurCutIdx'])]
        print('[orca-stop] camera staff=%s cut=%s'%(row,cut))

    flags=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtManager.mFlag.mFlag')
    def flag_on(value):
        return value>=0 and value<10240 and bool(int(flags[value//32])&(1<<(value%32)))
    for index in range(int(header['eventNum'])):
        event=events['mEventP'][index]
        if int(event['mEventState'])!=2:continue
        print('[orca-stop] active event %d %s'%(index,event['mName'].string()))
        for j in range(int(event['mNStaff'])):
            row=events['mStaffP'][int(event['mStaffIdx'][j])]
            ci=int(row['mCurCutIdx'])
            if ci<0:continue
            cut=events['mCutP'][ci]
            starts=[int(cut['mStartFlag'][k]) for k in range(3)]
            print('[orca-stop] staff=%s cut=%d %s start=%s/%s done=%s action=%s'%(row['mName'].string(),ci,cut['mName'].string(),starts,[flag_on(f) for f in starts],flag_on(int(cut['mFlagIdx'])),row['mbHasAction']))

            di=int(cut['mFirstDataIdx'])
            while 0<=di<int(events['mHeaderP']['dataNum']):
                data=events['mDataP'][di];kind=int(data['mSubstanceType']);off=int(data['mSubstanceIdx']);count=int(data['mSubstanceSize'])
                if kind in (0,1):value=[float(events['mFDataP'][off+k]) for k in range(count*(3 if kind==1 else 1))]
                elif kind==3:value=[int(events['mIDataP'][off+k]) for k in range(count)]
                elif kind==4:value=(events['mSDataP']+off).string()
                else:value='type %d'%kind
                print('[orca-stop]    %s=%s'%(data['mName'].string(),value))
                di=int(data['mNextIdx'])

    try:
        message=gdb.parse_and_eval("'d_a_npc_ji1.cpp'::l_msg")
        print('[orca-stop] message=%s status=%s select=%s'%(message['mMsgNo'],message['mStatus'],message['mSelectNum'])) if int(message) else print('[orca-stop] message null')
    except gdb.error as error:print('[orca-stop] message unavailable: %s'%error)
    player=gdb.parse_and_eval('((daPy_lk_c*)((camera_process_class*)g_dComIfG_gameInfo.play.mCameraInfo[0].mpCamera)->mCamera.mpPlayerActor)')
    print('[orca-stop] player proc=%s cut=%s item=%s trigger=%s button=%s position=%s'%(player['mCurProc'],player['mCutType'],player['mEquipItem'],player['mItemTrigger'],player['mItemButton'],tuple(float(player['current']['pos'][v]) for v in ('x','y','z'))))
