# Isolated Hero's Clothes cinematic diagnosis, NOT the normal story route.
# Starts at --outset then writes LinkRM point 200 room 0 layer 8 wipe 0.
# Uses A to advance text, validates the native particle selector/emitter,
# and checks hero clothes plus keyboard movement after control returns.
# ./tools/probe-pc.sh --outset --headless --deterministic --seconds 240 --gdb "$PWD/tools/clothes_scene.gdb"
set pagination off
set confirm off
handle SIGUSR1 SIGUSR2 SIGPIPE nostop noprint
python
import gdb
import struct
particle_emitted = False
selectors = 0
keybase = int(gdb.parse_and_eval("&'(anonymous namespace)::g_key'"))
class RoomTransition(gdb.Breakpoint):
    def __init__(self):
        super().__init__('daPy_lk_c::execute')
        self.outside, self.inside, self.origin, self.requested = 0, 0, None, False
        self.seen_demo, self.free_since = False, None
    def stop(self):
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
                        print('[clothes] SUCCESS: isolated clothes scene, valid particles, hero clothes and control return')
                        gdb.set_convenience_variable('tww_probe_success', 1)
                        return True
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
        print('[clothes] create particle group=%d resource=%d effect=%d' % (group,resource,effect))
        assert group < 16 and resource < 8
        if selectors: particle_emitted = True
        return False
selector_address = int(gdb.parse_and_eval("&'JStudio_JParticle::TAdaptor_particle::adaptor_do_PARTICLE(JStudio::data::TEOperationData, void const*, unsigned long)'"))
Selector('*0x%x' % selector_address)
Emit('JPAEmitterManager::createSimpleEmitterID')
RoomTransition()
end
source ../tools/crash.gdb
