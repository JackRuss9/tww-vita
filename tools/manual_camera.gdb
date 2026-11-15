# Keyboard-only native manual-camera regression. Isolated --outset start.
# Tests entry, both directions, pitch stops, angle wrap, idle, walking and L.
# Repeats in LinkRM via a next-stage request; not a door/story route test.
# ./tools/probe-pc.sh --outset --headless --deterministic --seconds 180 --gdb "$PWD/tools/manual_camera.gdb"
set pagination off
set confirm off
handle SIGUSR1 SIGUSR2 SIGPIPE nostop noprint
python
import gdb, math, struct
keybase=int(gdb.parse_and_eval("&'(anonymous namespace)::g_key'"))
phases=[('settle',180,None),('right',30,15),('hold',30,None),('left',30,13),
        ('down',90,14),('up',90,12),('wrap',120,15),('idle',45,None),
        ('walk',60,26),('walk_orbit',60,(26,13)),('L',5,20),('follow',60,None)]
expected_stage="sea"
phase=0
age=0
previous=None
samples={}
turn=0.0
close_checks=0

def angle(cam,name):return int(cam['mViewCache']['mDirection'][name]['mAngle'])*180/32768

def delta(a,b):return (a-b+180)%360-180

class Done(gdb.FinishBreakpoint):
    def __init__(self,address):
        super().__init__(gdb.newest_frame(),internal=True)
        self.address=address
    def stop(self):
        global phase,age,previous,turn,expected_stage,close_checks
        cam=gdb.parse_and_eval('((dCamera_c*)0x%x)'%self.address)
        name,length,key=phases[phase]
        player=cam['mpPlayerActor']
        if not player:return False
        if int(player.cast(gdb.lookup_type('daPy_lk_c').pointer())['mDemo']['mDemoType']):return False
        mode=int(cam['mCurMode'])
        pitch,yaw=angle(cam,'mAzimuth'),angle(cam,'mInclination')
        xyz=tuple(float(player['current']['pos'][x]) for x in ('x','y','z'))
        assert all(math.isfinite(float(cam['mViewCache'][f][x])) for f in ('mEye','mCenter') for x in ('x','y','z'))
        if mode==12:
            radius=float(cam['mDirection']['mRadius'])
            threshold=float(cam['mCamSetup']['mSubjLinkCullDist'])
            if radius<threshold:
                status=int(gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCameraInfo')[int(cam['mCameraID'])]['mCameraAttentionStatus'])
                assert status & 0x20
                close_checks+=1
        age+=1
        if age==1:
            samples[name]=(pitch,yaw,xyz)
            turn=0.0
        elif previous is not None:
            step=delta(yaw,previous)
            if name in ('right','left','wrap'):assert abs(step)<9
            turn+=step
        previous=yaw
        if age<length:return False
        print('[manual] '+expected_stage+' %s mode=%d style=%d pitch=%.3f yaw=%.3f turn=%.3f pos=%s' % (name,mode,int(cam['mCurStyle']),pitch,yaw,turn,xyz))
        if name in ('right','left','down','up','wrap','idle','walk','walk_orbit'):assert mode==12
        if name=='right':assert abs(turn)>30
        if name=='hold':assert abs(turn)<0.1
        if name=='left':assert turn < -30
        if name=='down':assert pitch < samples[name][0]-5
        if name=='up':assert pitch > samples[name][0]+5
        if name in ('right','left','down','up','wrap','idle'):
            assert sum((xyz[i]-samples[name][2][i])**2 for i in (0,2))**.5<1
        if name in ('down','up'):
            params=gdb.parse_and_eval('dCamParam_c::styles')[int(cam['mCurStyle'])]['styleParam']
            endpoints=sorted((float(params[16]),float(params[17])))
            assert endpoints[0]-0.02 <= pitch <= endpoints[1]+0.02
        if name=='wrap':assert abs(turn)>360
        if name=='idle':assert abs(turn)<0.1
        if name=='walk_orbit':assert abs(turn)>100
        if name in ('walk','walk_orbit'):assert sum((xyz[i]-samples[name][2][i])**2 for i in (0,2))**.5>10
        if name=='follow':
            assert mode!=12
            status=int(gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCameraInfo')[int(cam['mCameraID'])]['mCameraAttentionStatus'])
            assert not status & 0x20
            for code in (12,13,14,15,20,26):gdb.selected_inferior().write_memory(keybase+code,b'\0')
            if expected_stage=='sea':
                print('[manual] exterior passed; diagnostic next-stage request LinkRM point 1 room 0 wipe 10')
                request=int(gdb.parse_and_eval('&g_dComIfG_gameInfo.play.mNextStage'))
                gdb.selected_inferior().write_memory(request,struct.pack('<8shbbbb',b'LinkRM',1,0,-1,1,10))
                expected_stage='LinkRM'
                phase=0
                age=0
                previous=None
                return False
            assert close_checks>0
            print('[manual] close-view player hiding checks=%d'%close_checks)
            print('[manual] SUCCESS: exterior/interior C-stick orbit, pitch, wrap, idle, walking, L return and finite views')
            gdb.set_convenience_variable('tww_probe_success',1)
            return True
        phase+=1
        age=0
        return False

class Camera(gdb.Breakpoint):
    def stop(self):
        if gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCurStage.mName').string()!=expected_stage:
            for code in (12,13,14,15,20,26):gdb.selected_inferior().write_memory(keybase+code,b'\0')
            return False
        key=phases[phase][2]
        keys=key if isinstance(key,tuple) else (key,)
        for code in (12,13,14,15,20,26):gdb.selected_inferior().write_memory(keybase+code,bytes([code in keys]))
        sp=int(gdb.parse_and_eval('$esp'))
        address=struct.unpack('<I',bytes(gdb.selected_inferior().read_memory(sp+4,4)))[0]
        Done(address)
        return False
address=int(gdb.parse_and_eval("&'dCamera_c::Run()'"))
Camera('*0x%x'%address)
end
source ../tools/crash.gdb
