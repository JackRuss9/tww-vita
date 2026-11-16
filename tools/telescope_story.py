"""Input-only telescope tutorial observer used by aryll_story.gdb.

The parent driver earns the items/events and requests diagnostic stages.
This observer supplies native pad axes/buttons, never quest/camera state.
"""
import math
import os
import struct
import gdb


class Postmen(gdb.Breakpoint):
    def __init__(self, owner):
        super().__init__('daNpc_Bm1_c::_execute', internal=True)
        self.owner = owner

    def stop(self):
        actor = gdb.parse_and_eval('this')
        self.owner.postmen[int(actor['mBsPcId'])] = int(actor)
        return False


class FlyEvent(gdb.Breakpoint):
    def __init__(self, owner):
        super().__init__('dEvent_manager_c::order', internal=True)
        self.owner = owner

    def stop(self):
        index = int(gdb.parse_and_eval('eventIdx'))
        if index >= 0:
            manager = gdb.parse_and_eval('this')
            if manager['mList']['mEventP'][index]['mName'].string() == 'zelda_fly':
                self.owner.fly_seen = True
                print('[telescope] real zelda_fly event ordered')
        return False


class TelescopeStory:
    def __init__(self, keybase):
        self.keys = keybase
        self.axes = int(gdb.parse_and_eval("&'(anonymous namespace)::g_axis'"))
        self.postmen = {}
        self.phases = set()
        self.fly_seen = False
        self.free_age = 0
        self.origin = None
        Postmen(self)
        FlyEvent(self)

    def key(self, code, down):
        gdb.selected_inferior().write_memory(self.keys + code, bytes([bool(down)]))

    def step(self, player, aryll_address, age):
        for code in (26,22,4,7,44,82,25,225):
            self.key(code, False)
        gdb.selected_inferior().write_memory(self.axes, struct.pack('<hh',0,0))
        demo = int(player['mDemo']['mDemoType'])
        if demo:
            self.key(44, 91 <= age % 120 < 120)
            return False
        proc = int(player['mCurProc'])
        if self.fly_seen and proc in (4,6):
            self.free_age += 1
            xyz = tuple(float(player['current']['pos'][v]) for v in ('x','y','z'))
            if self.free_age == 30:
                self.origin = xyz
            self.key(26, 30 <= self.free_age < 60)
            if self.free_age >= 80 and self.origin:
                distance = sum((xyz[i]-self.origin[i])**2 for i in (0,2))**.5
                assert {1,2,3,4}.issubset(self.phases), self.phases
                assert distance > 10, distance
                print('[telescope] SUCCESS: tutorial, zelda_fly, following dialogue and movement=%.3f' % distance)
                if os.environ.get('TWW_TEST_ORCA')=='1':
                    checkpoint=os.environ.get('TWW_ORCA_SAVE_CHECKPOINT')
                    if checkpoint:
                        saved=gdb.parse_and_eval('g_dComIfG_gameInfo.save.mSavedata')
                        with open(checkpoint,'wb') as output:
                            output.write(bytes(gdb.selected_inferior().read_memory(int(saved.address),saved.type.sizeof)))
                        print('[orca] retained earned native savedata checkpoint locally')
                    from orca_story import OrcaStory
                    OrcaStory(self.keys)
                    self.free_age=-1000000
                    return False
                gdb.set_convenience_variable('tww_probe_success',1)
                return True
            return False
        if proc != 0:
            self.key(25, 5 <= age < 20)
            return False
        aryll = gdb.parse_and_eval('((daNpc_Ls1_c*)0x%x)' % aryll_address)
        tutorial = int(aryll['m833'])
        if tutorial not in self.phases:
            self.phases.add(tutorial)
            print('[telescope] tutorial phase=%d' % tutorial)
        status = int(gdb.parse_and_eval('g_dComIfG_gameInfo.play.mScopeMesgStatus'))
        self.key(44, status != 11 and 70 <= age % 120 < 100)
        target = self.postmen.get(int(aryll['mBm1ProcID']))
        if not target:
            return False
        actor = gdb.parse_and_eval('((daNpc_Bm1_c*)0x%x)' % target)
        point = [float(actor['attention_info']['position'][v]) for v in ('x','y','z')]
        if tutorial >= 4:
            point[1] += 2000
        look = gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCameraInfo[0].mpCamera->view.mLookat')
        eye = [float(look['mEye'][v]) for v in ('x','y','z')]
        front = [float(look['mCenter'][v])-eye[i] for i,v in enumerate(('x','y','z'))]
        def norm(vector):
            length = sum(v*v for v in vector)**.5
            assert length > 0
            return [v/length for v in vector]
        front = norm(front)
        right = norm([-front[2],0,front[0]])
        up = [right[1]*front[2]-right[2]*front[1],right[2]*front[0]-right[0]*front[2],right[0]*front[1]-right[1]*front[0]]
        offset = [point[i]-eye[i] for i in range(3)]
        dot = lambda vector: sum(offset[i]*vector[i] for i in range(3))
        depth,horizontal,vertical = dot(front),dot(right),dot(up)
        axis = [20000,0]
        if depth > 0:
            axis = []
            for error in (horizontal/depth,vertical/depth):
                # Raw PADClamp has a 15/100 dead zone: use >=20 for a
                # small real stick deflection, not an input lost to clamping.
                value = 0 if abs(error)<.0008 else math.copysign(min(.7,max(.20,abs(error)*5)),error)
                axis.append(round(value*32767))
        gdb.selected_inferior().write_memory(self.axes,struct.pack('<hh',*axis))
        zoom = tutorial == 2 or (tutorial == 1 and depth>0 and abs(horizontal/depth)<.01 and abs(vertical/depth)<.01)
        self.key(82,zoom)
        if age % 120 == 0:
            print('[telescope] age=%d UI=%d phase=%d' % (age,status,tutorial))
        return False
