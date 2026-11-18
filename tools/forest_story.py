"""Experimental input-only Outset forest continuation after the earned sword."""
import gdb
import math
import struct

class Bokoblin(gdb.Breakpoint):
    def __init__(self,owner):
        super().__init__('daBk_Execute',internal=True)
        self.owner=owner
    def stop(self):
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

class Tetra(gdb.Breakpoint):
    def __init__(self,owner):
        super().__init__('daNpc_Zl1_c::_execute',internal=True)
        self.owner=owner
    def stop(self):
        self.owner.tetra=int(gdb.parse_and_eval('this'))
        return False

class ForestStory(gdb.Breakpoint):
    def __init__(self,keybase):
        super().__init__('daPy_lk_c::execute',internal=True)
        self.keys=keybase
        self.axes=int(gdb.parse_and_eval("&'(anonymous namespace)::g_axis'"))
        self.age=0
        self.enemies={}
        self.defeated=set()
        self.tetra=None
        self.last=None
        Bokoblin(self);BokoblinDelete(self);Tetra(self)
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
        targets=[]
        for pid,address in self.enemies.items():
            enemy=gdb.parse_and_eval('((bk_class*)0x%x)'%address)
            if int(enemy['health'])<=0 or int(enemy['m121C']):continue
            point=[float(enemy['current']['pos'][v]) for v in ('x','y','z')]
            distance=sum((point[i]-xyz[i])**2 for i in (0,2))**.5
            # The two hanging ambushers start above the path; approach the
            # roaming Bokoblin first rather than walking into the cliff.
            priority=distance+(20000 if int(enemy['mType'])==7 and not self.defeated else 0)
            targets.append((priority,distance,point))
        if targets:
            _,distance,point=min(targets)
        elif self.tetra:
            actor=gdb.parse_and_eval('((daNpc_Zl1_c*)0x%x)'%self.tetra)
            point=[float(actor['current']['pos'][v]) for v in ('x','y','z')]
            distance=sum((point[i]-xyz[i])**2 for i in (0,2))**.5
        else:return False
        if targets and distance<280:
            self.key(20,True)
            if distance>140:self.key(26,True)
            self.key(225,self.age%24<5)
            self.key(44,int(gdb.parse_and_eval('g_dComIfG_gameInfo.play.mDoStatus'))==0x1a)
        elif not targets and distance<140:
            self.key(44,70<=self.age%120<95)
        else:
            look=gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCameraInfo[0].mpCamera->view.mLookat')
            fx=float(look['mCenter']['x'])-float(look['mEye']['x'])
            fz=float(look['mCenter']['z'])-float(look['mEye']['z'])
            dx,dz=point[0]-xyz[0],point[2]-xyz[2]
            forward,right=dx*fx+dz*fz,-dx*fz+dz*fx
            self.key((26 if forward>0 else 22) if abs(forward)>abs(right) else (7 if right>0 else 4),True)
        return False
