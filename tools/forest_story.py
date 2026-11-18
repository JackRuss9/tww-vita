"""Experimental input-only Outset forest continuation after the earned sword."""
import gdb
import math
import os
import json
import struct

class Bokoblin(gdb.Breakpoint):
    def __init__(self,owner):
        super().__init__('daBk_Execute',internal=True)
        self.owner=owner
    def stop(self):
        if gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCurStage.mName').string()!='A_mori':return False
        actor=gdb.parse_and_eval('i_this')
        pid=int(actor['mBsPcId'])
        if pid not in self.owner.enemies:print('[forest] Bokoblin id=%d type=%s health=%s'%(pid,actor['mType'],actor['health']))
        self.owner.enemies[pid]=int(actor)
        if int(actor['health'])<=0 and pid not in self.owner.defeated:
            self.owner.defeated.add(pid)
            print('[forest] Bokoblin health depleted id=%d'%pid)
        return False

class BokoblinDelete(gdb.Breakpoint):
    def __init__(self,owner):
        super().__init__('daBk_Delete',internal=True)
        self.owner=owner
    def stop(self):
        actor=gdb.parse_and_eval('i_this');pid=int(actor['mBsPcId'])
        self.owner.enemies.pop(pid,None)
        print('[forest] Bokoblin deleted id=%d health=%s'%(pid,actor['health']))
        return False

class Bird(gdb.Breakpoint):
    def __init__(self,owner):
        super().__init__('daBb_Execute',internal=True)
        self.owner=owner
        self.last={}
    def stop(self):
        if gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCurStage.mName').string()!='A_mori':return False
        actor=gdb.parse_and_eval('i_this')
        pid=int(actor['actor']['mBsPcId'])
        self.owner.birds[pid]=int(actor)
        state=tuple(int(actor[f]) for f in ('unk_2DF','unk_2F0','unk_2F1','unk_35E','unk_2E0'))
        if self.last.get(pid)!=state:
            print('[forest-bird] id=%d carry/wing/path/point/drop=%s target=%s'%(pid,state,actor['unk_2F4']))
            self.last[pid]=state
        return False

class Tetra(gdb.Breakpoint):
    def __init__(self,owner):
        super().__init__('daNpc_Zl1_c::_execute',internal=True)
        self.owner=owner
    def stop(self):
        self.owner.tetra=int(gdb.parse_and_eval('this'))
        return False

class CurveDone(gdb.FinishBreakpoint):
    def __init__(self,camera,frame,values,previous):
        super().__init__(gdb.newest_frame(),internal=True)
        self.camera,self.frame,self.values,self.previous=camera,frame,values,previous
    def stop(self):
        camera=gdb.parse_and_eval('((dCamera_c*)0x%x)'%self.camera)
        timer=self.values['Timer'][0]
        curve=self.values.get('BSpCurve',[1])[0]
        if curve and timer>1:
            t=min(2,2*self.frame/(timer-1))
            ratio=.5*t*t if t<=1 else 1-.5*(2-t)**2
        else:ratio=min(1,(self.frame+1)/timer) if timer>0 else 1
        cushion=self.values.get('Cushion',[1])[0]
        for field,start,end in [('mEye','StartEye','Eye'),('mCenter','StartCenter','Center')]:
            target=[self.values[start][i]+ratio*(self.values[end][i]-self.values[start][i]) for i in range(3)]
            expected=[self.previous[field][i]+cushion*(target[i]-self.previous[field][i]) for i in range(3)]
            actual=[float(camera['mViewCache'][field][v]) for v in ('x','y','z')]
            assert max(abs(actual[i]-expected[i]) for i in range(3))<.01,(field,self.frame,actual,expected)
        fovy=self.values['StartFovy'][0]+ratio*(self.values['Fovy'][0]-self.values['StartFovy'][0])
        expected=self.previous['mFovy']+cushion*(fovy-self.previous['mFovy'])
        assert abs(float(camera['mViewCache']['mFovy'])-expected)<.002
        assert all(int(camera[f]) for f in ('m100','m101','m102'))
        assert bool(self.return_value)==(self.frame>=timer)
        if self.frame in (0,timer//2,timer-1,timer):print('[forest-camera] UNITRANS frame=%d/%d curve=%s ratio=%.6f view/FOV/readiness/end verified'%(self.frame,timer,curve,ratio))
        return False

class CurveFrame(gdb.Breakpoint):
    def __init__(self):
        super().__init__('dCamera_c::uniformTransEvCamera',internal=True)
        self.cuts={}
    def stop(self):
        if gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCurStage.mName').string()!='A_mori':return False
        camera=gdb.parse_and_eval('this')
        events=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtManager.mList')
        staff=int(camera['mEventData']['mStaffIdx'])
        if staff<0:return False
        cut=int(events['mStaffP'][staff]['mCurCutIdx']);key=(int(events['mHeaderP']),cut)
        frame=int(camera['m11C'])
        if frame==0:
            di=int(events['mCutP'][cut]['mFirstDataIdx']);values={}
            while 0<=di<int(events['mHeaderP']['dataNum']):
                row=events['mDataP'][di];kind=int(row['mSubstanceType']);off=int(row['mSubstanceIdx']);count=int(row['mSubstanceSize'])
                if kind in (0,1):values[row['mName'].string()]=[float(events['mFDataP'][off+i]) for i in range(count*(3 if kind==1 else 1))]
                elif kind==3:values[row['mName'].string()]=[int(events['mIDataP'][off+i]) for i in range(count)]
                elif kind==4:values[row['mName'].string()]=(events['mSDataP']+off).string()
                di=int(row['mNextIdx'])
            for name,field in [('StartEye','mEye'),('Eye','mEye'),('StartCenter','mCenter'),('Center','mCenter')]:values.setdefault(name,[float(camera[field][v]) for v in ('x','y','z')])
            for name in ('StartFovy','Fovy'):values.setdefault(name,[float(camera['mFovy'])])
            self.cuts[key]=values
        values=self.cuts[key]
        if 'RelActor' in values or values.get('TransType',[0])!=[0] or 'Timer' not in values:return False
        previous={f:[float(camera['mViewCache'][f][v]) for v in ('x','y','z')] for f in ('mEye','mCenter')}
        previous['mFovy']=float(camera['mViewCache']['mFovy'])
        CurveDone(int(camera),frame,values,previous)
        return False

class RestoreDone(gdb.FinishBreakpoint):
    def __init__(self,camera,frame):
        super().__init__(gdb.newest_frame(),internal=True)
        self.camera,self.frame=camera,frame
    def stop(self):
        camera=gdb.parse_and_eval('((dCamera_c*)0x%x)'%self.camera)
        assert all(int(camera[f]) for f in ('m100','m101','m102')), 'RESTOREPOS not ready'
        for field in ('mEye','mCenter'):
            assert all(math.isfinite(float(camera['mViewCache'][field][v])) for v in ('x','y','z'))
        assert math.isfinite(float(camera['mViewCache']['mFovy']))
        if self.return_value:
            print('[forest-camera] RESTOREPOS completed frame=%d; finite view and readiness verified'%self.frame)
        return False

class RestoreFrame(gdb.Breakpoint):
    def __init__(self):
        super().__init__('dCamera_c::restorePosEvCamera',internal=True)
    def stop(self):
        if gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCurStage.mName').string()!='A_mori':return False
        camera=gdb.parse_and_eval('this')
        RestoreDone(int(camera),int(camera['m11C']))
        return False

class ForestStory(gdb.Breakpoint):
    def __init__(self,keybase):
        super().__init__('daPy_lk_c::execute',internal=True)
        self.keys=keybase
        self.axes=int(gdb.parse_and_eval("&'(anonymous namespace)::g_axis'"))
        self.age=0
        self.enemies={}
        self.birds={}
        self.defeated=set()
        self.tetra=None
        self.last=None
        self.route=[]
        self.route_index=0
        self.after_route_loaded=False
        if os.environ.get('TWW_FOREST_ROUTE'):
            with open(os.environ['TWW_FOREST_ROUTE']) as stream:self.route=json.load(stream)
            print('[forest] input navigation route loaded, %d waypoints'%len(self.route))
        Bokoblin(self);BokoblinDelete(self);Tetra(self);CurveFrame();RestoreFrame();Bird(self)
        for code in (26,22,4,7,44,225,20):self.key(code,False)
        request=int(gdb.parse_and_eval('&g_dComIfG_gameInfo.play.mNextStage'))
        gdb.selected_inferior().write_memory(request,struct.pack('<8shbbbb',b'A_mori',0,0,-1,1,0))
        print('[forest] diagnostic request A_mori point 0; earned sword/story state retained')
    def key(self,code,on):gdb.selected_inferior().write_memory(self.keys+code,bytes([bool(on)]))
    def stop(self):
        stage=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCurStage.mName').string()
        if stage!='A_mori':return False
        self.age+=1
        player=gdb.parse_and_eval('this')
        for code in (26,22,4,7,44,225,20):self.key(code,False)
        gdb.selected_inferior().write_memory(self.axes,struct.pack('<hh',0,0))
        xyz=[float(player['current']['pos'][v]) for v in ('x','y','z')]
        assert all(math.isfinite(v) for v in xyz)
        demo=int(player['mDemo']['mDemoType'])
        camera=gdb.parse_and_eval('((camera_process_class*)g_dComIfG_gameInfo.play.mCameraInfo[0].mpCamera)->mCamera')
        staff=int(camera['mEventData']['mStaffIdx'])
        cut_name='none'
        if staff>=0:
            events=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtManager.mList')
            cut=int(events['mStaffP'][staff]['mCurCutIdx'])
            if cut>=0:cut_name=events['mCutP'][cut]['mName'].string()
        state=(demo,int(player['mCurProc']),cut_name,len(self.enemies),len(self.defeated))
        if state!=self.last and (demo or self.age%30==0):print('[forest] demo/proc/camera/enemies/defeated=%s'%(state,));self.last=state
        if self.age%120==0:print('[forest] age=%d pos=%s camera-ready=%s/%s/%s'%(self.age,xyz,camera['m100'],camera['m101'],camera['m102']))
        if demo:
            self.key(44,70<=self.age%120<95)
            return False
        if os.environ.get('TWW_FOREST_FIRST_FIGHT')=='1' and self.defeated and not demo:
            print('[forest] SUCCESS: intro, walking route, restored event camera and first Bokoblin defeated by normal attacks; rescue not tested')
            gdb.set_convenience_variable('tww_probe_success',1)
            return True
        targets=[]
        for pid,address in self.enemies.items():
            enemy=gdb.parse_and_eval('((bk_class*)0x%x)'%address)
            if int(enemy['health'])<=0 or int(enemy['m121C']):continue
            point=[float(enemy['current']['pos'][v]) for v in ('x','y','z')]
            distance=sum((point[i]-xyz[i])**2 for i in (0,2))**.5
            # The two hanging ambushers start above the path; approach the
            # roaming Bokoblin first rather than walking into the cliff.
            priority=distance+(20000 if int(enemy['mType'])!=4 and not self.defeated else 0)
            targets.append((priority,distance,point))
        if self.defeated and not self.after_route_loaded and os.environ.get('TWW_FOREST_AFTER_ROUTE'):
            with open(os.environ['TWW_FOREST_AFTER_ROUTE']) as stream:self.route=json.load(stream)
            self.route_index=0
            self.after_route_loaded=True
            print('[forest] post-fight input route loaded, %d waypoints'%len(self.route))
        navigating=False
        while (not self.defeated or self.after_route_loaded) and self.route_index<len(self.route):
            point=self.route[self.route_index]
            distance=sum((point[i]-xyz[i])**2 for i in (0,2))**.5
            if distance<60 and (not self.after_route_loaded or abs(point[1]-xyz[1])<120):
                self.route_index+=1
                print('[forest] walking waypoint %d/%d'%(self.route_index,len(self.route)))
            else:
                navigating=True
                break
        if navigating:
            if self.after_route_loaded:self.key(44,self.age%90<5)
        elif targets:
            _,distance,point=min(targets)
        elif self.tetra:
            actor=gdb.parse_and_eval('((daNpc_Zl1_c*)0x%x)'%self.tetra)
            point=[float(actor['current']['pos'][v]) for v in ('x','y','z')]
            distance=sum((point[i]-xyz[i])**2 for i in (0,2))**.5
        else:return False
        if not navigating and targets and distance<280:
            self.key(20,True)
            if distance>140:self.key(26,True)
            self.key(225,self.age%24<5)
            self.key(44,int(gdb.parse_and_eval('g_dComIfG_gameInfo.play.mDoStatus'))==0x1a)
        elif not navigating and not targets and distance<140:
            self.key(44,70<=self.age%120<95)
        else:
            look=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCameraInfo[0].mpCamera->view.mLookat')
            fx=float(look['mCenter']['x'])-float(look['mEye']['x'])
            fz=float(look['mCenter']['z'])-float(look['mEye']['z'])
            dx,dz=point[0]-xyz[0],point[2]-xyz[2]
            forward,right=dx*fx+dz*fz,-dx*fz+dz*fx
            self.key((26 if forward>0 else 22) if abs(forward)>abs(right) else (7 if right>0 else 4),True)
        return False


def describe():
    owners=[bp for bp in gdb.breakpoints() if type(bp).__name__=='ForestStory']
    for owner in owners:
        print('[forest-diagnostic] age=%s defeated=%s'%(owner.age,owner.defeated))
        for pid,address in owner.enemies.items():
            actor=gdb.parse_and_eval('((bk_class*)0x%x)'%address)
            xyz=tuple(float(actor['current']['pos'][v]) for v in ('x','y','z'))
            print('[forest-diagnostic] enemy=%d type=%s health=%s action=%s pos=%s'%(pid,actor['mType'],actor['health'],actor['dr']['mAction'],xyz))
        for pid,address in owner.birds.items():
            actor=gdb.parse_and_eval('((bb_class*)0x%x)'%address)
            print('[forest-diagnostic] bird=%d pos=%s target=%s path=%s point=%s mode=%s'%(pid,actor['actor']['current']['pos'],actor['unk_2F4'],actor['ppd'],actor['unk_35E'],actor['unk_2F1']))
        if owner.tetra:
            actor=gdb.parse_and_eval('((daNpc_Zl1_c*)0x%x)'%owner.tetra)
            print('[forest-diagnostic] Tetra=%s'%(tuple(float(actor['current']['pos'][v]) for v in ('x','y','z')),))
