# Exercise the real startup using virtual keyboard buttons only. No scene,
# save-state, event flag, position or name data is changed by this driver.
# tools/probe-pc.sh --headless --deterministic --seconds 300 --gdb "$PWD/tools/normal_flow.gdb"
set pagination off
set confirm off
set print static-members off
handle SIGUSR1 SIGUSR2 SIGPIPE nostop noprint
python
import gdb
import json
import os
keys = {'a': 44, 'start': 40, 'left': 80, 'up': 26}
keybase = int(gdb.parse_and_eval("&'(anonymous namespace)::g_key'"))
opening_done = False
seen_game_demo = False
free_frames = 0
free_since = None
move_origin = None
camera_cut = None

# Opt-in render diagnosis. Capture only display data; never change the
# camera, actor positions, event flags or simulation clock.
# TWW_CAPTURE_TOWER=sample: five 60-frame bursts; =all: first 4000 frames.
class TowerFrames(gdb.Breakpoint):
    def __init__(self, mode):
        super().__init__('(anonymous namespace)::dumpFrame(void const*)')
        self.mode, self.age = mode, 0
        self.path = os.path.join(os.environ['SMS_GX_DUMP_DIR'], 'camera.jsonl')

    def stop(self):
        if not opening_done:
            return False
        stage = gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCurStage.mName').string()
        if stage != 'sea':
            return False
        camera = gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCameraInfo[0].mpCamera')
        if int(camera) == 0:  # Stage name changes before its camera is created.
            return False
        self.age += 1
        capture = (self.age <= 4000 if self.mode == 'all' else
                   any(first <= self.age < last for first, last in
                       ((20, 80), (250, 310), (700, 760), (1500, 1560), (2400, 2460))))
        gdb.execute('set variable every = %d' % int(capture))
        if capture:
            # These are the actual rendered view values. CameraInfo's
            # cached mCameraPos/mCameraTarget remain zero in this path.
            view = camera.dereference()['view']
            xyz = lambda field: [float(view['mLookat'][field][axis]) for axis in ('x', 'y', 'z')]
            row = dict(scene_frame=self.age, gx_frame=int(gdb.parse_and_eval('s_frame')),
                       eye=xyz('mEye'), target=xyz('mCenter'), fovy=float(view['mFovy']),
                       near=float(view['mNear']), far=float(view['mFar']))
            with open(self.path, 'a') as output:
                output.write(json.dumps(row) + '\n')
        return False

capture_mode = os.environ.get('TWW_CAPTURE_TOWER')
if capture_mode:
    if capture_mode not in ('sample', 'all'):
        raise gdb.GdbError('TWW_CAPTURE_TOWER must be sample or all')
    TowerFrames(capture_mode)

def press(key):
    for name, code in keys.items():
        gdb.selected_inferior().write_memory(keybase + code, bytes([name == key]))

def val(obj, name):
    return int(obj[name])

class Flow(gdb.Breakpoint):
    def __init__(self, name, kind):
        super().__init__(name)
        self.kind, self.n, self.last, self.age = kind, 0, None, 0
        self.stride = {'link': 30, 'camera': 10, 'opening': 15, 'message': 5}.get(kind, 1)
    def stop(self):
        global opening_done, seen_game_demo, free_frames, free_since, move_origin, camera_cut
        self.n += 1
        # Returning early keeps the real execution count and avoids expensive
        # memory reads on every frame. GDB breakpoint state is not modified
        # from stop(), where changes to ignore_count are not reliable.
        if (self.n - 1) % self.stride:
            return False
        obj = gdb.parse_and_eval('i_Msg' if self.kind == 'message' else 'this')
        if self.kind == 'title':
            press('start' if 120 <= self.n % 180 < 136 else None)
            if self.n % 180 == 120:
                print('[flow] title Start, mode=%d' % val(obj, 'mEnterMode'))
        elif self.kind == 'name':
            state = tuple(val(obj, f) for f in ('mMainProc', 'mDrawProc', 'mMemCardCheckProc'))
            if state != self.last:
                self.last, self.age = state, 0
                print('[flow] Name main/draw/card=%s' % (state,))
            self.age += 1
            main, key = state[0], None
            if main == 7:
                if 60 <= self.age < 68 or 240 <= self.age < 248:
                    key = 'a'
                elif 180 <= self.age < 188:
                    key = 'start'
            elif main in (0, 2):
                cycle = self.age % 180
                if 45 <= cycle < 53:
                    key = 'left'
                elif 100 <= cycle < 108:
                    key = 'a'
                if self.age % 120 == 0:
                    err = obj['dFe_c'].dereference()
                    print('[flow] card state=%s command=%s error=%d/%d yes=%d' % (
                        gdb.parse_and_eval('g_mDoMemCd_control.field_0x1660'),
                        gdb.parse_and_eval('g_mDoMemCd_control.mCommand'),
                        val(err, 'mState'), val(err, 'mStatus'), val(err, 'mYesNo')))
            elif main == 4 and 60 <= self.age % 180 < 68:
                key = 'a'
            press(key)
        elif self.kind == 'message':
            # Gameplay and cinematic messages have separate implementations
            # sharing this function name, with different derived layouts.
            gameplay = obj.type.target().tag == 'sub_msg_class'
            state = val(obj, 'mStatus' if gameplay else 'field_0x164')
            if state != self.last:
                self.last = state
                if gameplay:
                    print('[flow] gameplay dialogue status=%d message=%d' % (state, val(obj, 'mMsgNo')))
                else:
                    control = gdb.parse_and_eval('dMesg_gpControl')
                    print('[flow] dialogue state=%d group=%d index=%d' %
                          (state, val(control, 'mGroupID'), val(control, 'mMessageIndex')))
        elif self.kind == 'opening':
            state = val(obj, 'mState')
            if state != self.last:
                self.last = state
                print('[flow] opening state=%d' % state)
            press(None) # Let every chapter play, including its text.
        elif self.kind == 'opening_end':
            if not opening_done:
                print('[flow] illustrated opening complete')
            opening_done = True
        elif self.kind == 'event':
            idx = int(gdb.parse_and_eval('eventIdx'))
            if idx >= 0:
                event = obj['mList']['mEventP'][idx]
                print('[flow] event %d %s' % (idx, event['mName'].string()))
        elif self.kind == 'params':
            size = int(gdb.parse_and_eval('p2'))
            ptr = int(gdb.parse_and_eval('p1'))
            ident = int(gdb.parse_and_eval('p0'))
            raw = bytes(gdb.selected_inferior().read_memory(ptr, min(size, 40))) if ptr else b''
            state = (ident, raw)
            if state != self.last and self.n < 100:
                self.last = state
                print('[flow] actor parameters id=%d size=%d bytes=%s' % (ident, size, raw.hex()))
        elif self.kind == 'camera':
            staff_id = val(obj['mEventData'], 'mStaffIdx')
            events = gdb.parse_and_eval('g_dComIfG_gameInfo.play.mEvtManager.mList')
            if staff_id >= 0 and int(events['mStaffP']) != 0:
                staff = events['mStaffP'][staff_id]
                ci = val(staff, 'mCurCutIdx')
                key = (int(events['mHeaderP']), ci)
                if key != camera_cut:
                    camera_cut = key
                    cut = events['mCutP'][ci]
                    print('[flow] CAMERA cut %d %s' % (ci, cut['mName'].string()))
                    di = val(cut, 'mFirstDataIdx')
                    while di >= 0:
                        data = events['mDataP'][di]
                        typ, off, n = (val(data, x) for x in ('mSubstanceType','mSubstanceIdx','mSubstanceSize'))
                        if typ in (0,1):
                            values = tuple(float(events['mFDataP'][off+i]) for i in range(n*(3 if typ==1 else 1)))
                        elif typ == 3:
                            values = tuple(int(events['mIDataP'][off+i]) for i in range(n))
                        elif typ == 4:
                            values = (events['mSDataP']+off).string()
                        else:
                            values = ('type', typ)
                        print('[flow]   %s = %s' % (data['mName'].string(), values))
                        di = val(data, 'mNextIdx')
        elif self.kind == 'link':
            pos = obj['current']['pos']
            xyz = tuple(float(pos[x]) for x in ('x','y','z'))
            demo = val(obj['mDemo'],'mDemoType')
            stage = gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCurStage.mName').string()
            if self.n % 120 == 1:
                print('[flow] Link stage=%s pos=%s proc=%d demo=%d/%d' %
                      (stage, xyz, val(obj,'mCurProc'), demo, val(obj['mDemo'],'mDemoMode')))
            if opening_done and stage == 'sea':
                seen_game_demo |= demo != 0
                if seen_game_demo and demo == 0:
                    if free_since is None: free_since = self.n
                    free_frames = self.n - free_since
                else:
                    free_since = None
                    free_frames = 0
                if free_frames == 60:
                    move_origin = xyz
                    print('[flow] control handed to Link; testing movement')
                key = 'up' if 60 <= free_frames < 90 else None
                if demo != 0 and 91 <= self.n % 120 < 120:
                    key = 'a' # Normal dialogue advance; never Start to skip a movie.
                press(key)
                if free_frames >= 100 and move_origin:
                    distance = sum((xyz[i]-move_origin[i])**2 for i in (0,2))**0.5
                    print('[flow] normal-start movement distance=%.3f' % distance)
                    if distance > 10:
                        print('[flow] SUCCESS: normal startup, named new game, full opening, control and movement')
                        gdb.set_convenience_variable('tww_probe_success', 1)
                        return True
        return False

for name, kind in [('dMsg_Execute','message'),
                   ('dDemo_actor_c::JSGSetData','params'),
                   ('daTitle_proc_c::proc_execute','title'),
                   ('dScnName_c::execute','name'),
                   ('dScnOpen_proc_c::proc_execute','opening'),
                   ('dScnOpen_c::changeGameScene','opening_end'),
                   ('dEvent_manager_c::order','event'),
                   ('dCamera_c::eventCamera','camera'),
                   ('daPy_lk_c::execute','link')]:
    Flow(name, kind)
end
source ../tools/crash.gdb
python
try:
    gdb.execute('frame 0')
    gdb.execute('info locals')
    prm = gdb.parse_and_eval('demo_actor->mPrm')
    print('[flow] stopped demo parameter:', prm)
    gdb.execute('x/40bx 0x%x' % int(prm['mData']))
except gdb.error as e:
    print('[flow] demo parameter unavailable:', e)
end
