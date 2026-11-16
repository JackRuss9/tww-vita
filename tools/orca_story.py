"""Experimental continuation from earned Outset state into Orca's house.

Entry/dialogue/PARTNER framing are checked. Training is NOT validated:
Ji1_linkmove currently blocks on the unimplemented UNITRANS camera.
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
        while di>=0:
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
        self.prep_age=0
        self.phases=set()
        self.present=False
        self.free_age=0
        OrcaActor(self)
        DoorActor(self)
        Present(self)
        PartnerFrame(self)
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
        if self.age%120==0:print('[orca] distance=%.1f proc=%s cut=%s'%(distance,player['mCurProc'],player['mCutType']))
        if self.present and equip==0x38 and 'teachAction' not in action:
            self.free_age+=1
            if self.free_age==30:self.origin=xyz
            self.key(26,30<=self.free_age<60)
            if self.free_age>=80:
                distance=sum((xyz[i]-self.origin[i])**2 for i in (0,2))**.5
                assert {0,1,2,3,4,5}.issubset(self.phases),self.phases
                assert distance>10,distance
                print('[orca] SUCCESS: earned sword presentation/inventory, training phases and movement=%.3f'%distance)
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
            if phase<=2:self.key(225,self.age%30<5)
            elif phase==3:self.key(225,self.phase_age%120<75)
            elif phase==4:
                status=int(gdb.parse_and_eval('g_dComIfG_gameInfo.play.mAStatus'))
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
        if owner.npc:
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
            while di>=0:
                data=events['mDataP'][di];kind=int(data['mSubstanceType']);off=int(data['mSubstanceIdx']);count=int(data['mSubstanceSize'])
                if kind in (0,1):value=[float(events['mFDataP'][off+k]) for k in range(count*(3 if kind==1 else 1))]
                elif kind==3:value=[int(events['mIDataP'][off+k]) for k in range(count)]
                elif kind==4:value=(events['mSDataP']+off).string()
                else:value='type %d'%kind
                print('[orca-stop]    %s=%s'%(data['mName'].string(),value))
                di=int(data['mNextIdx'])
