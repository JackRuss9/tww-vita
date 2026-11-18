"""Read-only per-frame checks of the beach's spherical actor-relative camera."""
import gdb
import math

actors = {}

def xyz(value):
    return [float(value[a]) for a in ('x', 'y', 'z')]

def arguments(camera):
    events = gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtManager.mList')
    staff = int(camera['mEventData']['mStaffIdx'])
    cut = int(events['mStaffP'][staff]['mCurCutIdx'])
    index = int(events['mCutP'][cut]['mFirstDataIdx'])
    values = {}
    while 0 <= index < int(events['mHeaderP']['dataNum']):
        row = events['mDataP'][index]
        kind, offset, count = (int(row[f]) for f in ('mSubstanceType', 'mSubstanceIdx', 'mSubstanceSize'))
        name = row['mName'].string()
        if kind in (0, 1):
            values[name] = [float(events['mFDataP'][offset+i]) for i in range(count*(3 if kind == 1 else 1))]
        elif kind == 3:
            values[name] = [int(events['mIDataP'][offset+i]) for i in range(count)]
        elif kind == 4:
            values[name] = (events['mSDataP']+offset).string()
        index = int(row['mNextIdx'])
    return cut, values

class Tetra(gdb.Breakpoint):
    def __init__(self):
        super().__init__('daNpc_Zl1_c::_execute', internal=True)
    def stop(self):
        actor = gdb.parse_and_eval('this')
        actors[int(actor['mBsPcId'])] = int(actor)
        return False

class Done(gdb.FinishBreakpoint):
    completed = set()
    def __init__(self, camera, frame, timer, expected, fovy, cut):
        super().__init__(gdb.newest_frame(), internal=True)
        self.camera, self.frame, self.timer = camera, frame, timer
        self.expected, self.fovy, self.cut = expected, fovy, cut
    def stop(self):
        camera = gdb.parse_and_eval('(dCamera_c*)0x%x' % self.camera)
        for name, expected in self.expected.items():
            actual = xyz(camera['mViewCache'][name])
            # Native angle lookup/short quantization and large world positions
            # differ slightly from this independent double-precision geometry.
            tolerance = 1.0 if name == 'mEye' else .1
            assert max(abs(a-b) for a,b in zip(actual,expected)) < tolerance, (self.cut,self.frame,name,actual,expected)
        assert abs(float(camera['mViewCache']['mFovy'])-self.fovy) < .002
        assert all(int(camera[f]) for f in ('m100','m101','m102'))
        assert bool(self.return_value) == (self.frame >= self.timer)
        if self.return_value: Done.completed.add(self.timer)
        if self.frame in (0,self.timer//2,self.timer-1,self.timer):
            print('[beach-camera] cut=%d frame=%d/%d spherical view/FOV/readiness/end verified' % (self.cut,self.frame,self.timer))
        return False

class Transition(gdb.Breakpoint):
    def __init__(self):
        super().__init__('dCamera_c::uniformTransEvCamera', internal=True)
        self.cuts = {}
    def stop(self):
        camera = gdb.parse_and_eval('this')
        cut, values = arguments(camera)
        if values.get('TransType',[0]) != [1] or values.get('RelActor') != '@STARTER': return False
        if values.get('RelUseMask') != '--oo': return False
        timer = int(values['Timer'][0]); frame = int(camera['m11C'])
        if frame == 0:
            for name, field in [('StartCenter','mCenter'),('StartEye','mEye')]:
                values.setdefault(name,xyz(camera[field]))
            values.setdefault('StartFovy',[float(camera['mFovy'])])
            values.setdefault('Fovy',[float(camera['mFovy'])])
            self.cuts[cut] = values
        values = self.cuts[cut]
        pid = int(gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtCtrl.mPt1'))
        assert pid in actors, ('Starter not captured',pid)
        actor = gdb.parse_and_eval('(fopAc_ac_c*)0x%x' % actors[pid])
        anchor = xyz(actor['attention_info']['position'])
        yaw = int(actor['shape_angle']['y'])*math.pi/32768
        def relative(point):
            x,y,z = point
            return [anchor[0]+x*math.cos(yaw)+z*math.sin(yaw),anchor[1]+y,anchor[2]-x*math.sin(yaw)+z*math.cos(yaw)]
        start, first_eye = values['StartCenter'], values['StartEye']
        end, last_eye = relative(values['Center']), relative(values['Eye'])
        if values.get('BSpCurve',[1])[0] and timer > 1:
            t = min(2,2*frame/(timer-1))
            ratio = .5*t*t if t <= 1 else 1-.5*(2-t)**2
        else: ratio = min(1,(frame+1)/timer)
        cushion = values.get('Cushion',[1])[0]
        previous = camera['mViewCache']
        center = [float(previous['mCenter'][a])+cushion*(start[i]+ratio*(end[i]-start[i])-float(previous['mCenter'][a])) for i,a in enumerate(('x','y','z'))]
        def globe(eye, center):
            x,y,z = [eye[i]-center[i] for i in range(3)]
            return math.sqrt(x*x+y*y+z*z), math.atan2(y,math.hypot(x,z)), math.atan2(x,z)
        first, last = globe(first_eye,start), globe(last_eye,end)
        radius = first[0]+ratio*(last[0]-first[0])
        angles = [first[i]+ratio*((last[i]-first[i]+math.pi)%(2*math.pi)-math.pi) for i in (1,2)]
        pitch,yaw = angles
        offset = [radius*math.cos(pitch)*math.sin(yaw),radius*math.sin(pitch),radius*math.cos(pitch)*math.cos(yaw)]
        eye = [float(previous['mEye'][a])+cushion*(center[i]+offset[i]-float(previous['mEye'][a])) for i,a in enumerate(('x','y','z'))]
        target_fovy = values['StartFovy'][0]+ratio*(values['Fovy'][0]-values['StartFovy'][0])
        fovy = float(previous['mFovy'])+cushion*(target_fovy-float(previous['mFovy']))
        Done(int(camera),frame,timer,{'mEye':eye,'mCenter':center},fovy,cut)
        return False

def install():
    Tetra()
    Transition()

def describe():
    """Read actor/message state without invoking game code in the inferior."""
    event = gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtCtrl')
    target = int(event['mPtItem'])
    print('[beach-stop] event=%s starter=%s target=%s talk=%s' % (event['mEventId'],event['mPt1'],event['mPtItem'],event['mPtTalk']))
    node = gdb.parse_and_eval('g_fopAcTg_Queue.mpHead')
    seen = set()
    while int(node) and int(node) not in seen:
        seen.add(int(node))
        tag = node.cast(gdb.lookup_type('create_tag_class').pointer())
        actor = tag['mpTagData'].cast(gdb.lookup_type('fopAc_ac_c').pointer())
        if int(actor['mBsPcId']) == target:
            npc = actor.cast(gdb.lookup_type('fopNpc_npc_c').pointer())
            print('[beach-stop] target NPC message=%s id=%s pointer=%s cut=%s' % (npc['mCurrMsgNo'],npc['mCurrMsgBsPcId'],npc['mpCurrMsg'],npc['mEventCut']))
            if int(npc['mpCurrMsg']):
                print('[beach-stop] message status=%s' % npc['mpCurrMsg']['mStatus'])
            break
        node = node['mpNextNode']
