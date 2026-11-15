# Isolated loading/fader diagnosis, NOT a normal story/door traversal test.
# Writes the next-stage request for the LinkRM destination/wipe after --outset.
# Does not exercise the door interaction or change inventory/event flags.
# ./tools/probe-pc.sh --outset --headless --seconds 120 --gdb "$PWD/tools/room_transition.gdb"
set pagination off
set confirm off
handle SIGUSR1 SIGUSR2 SIGPIPE nostop noprint
python
import gdb
import struct
keybase = int(gdb.parse_and_eval("&'(anonymous namespace)::g_key'"))
class RoomTransition(gdb.Breakpoint):
    def __init__(self):
        super().__init__('daPy_lk_c::execute')
        self.outside, self.inside, self.origin, self.requested = 0, 0, None, False
    def stop(self):
        obj = gdb.parse_and_eval('this')
        stage = gdb.parse_and_eval('g_dComIfG_gameInfo.play.mCurStage.mName').string()
        if stage == 'sea' and not self.requested:
            self.outside += 1
            if self.outside == 180:
                self.requested = True
                print('[room] diagnostic scene request: LinkRM point 1 room 0 wipe 10')
                request = int(gdb.parse_and_eval('&g_dComIfG_gameInfo.play.mNextStage'))
                gdb.selected_inferior().write_memory(request, struct.pack('<8shbbbb', b'LinkRM', 1, 0, -1, 1, 10))
        elif stage == 'LinkRM':
            self.inside += 1
            xyz = tuple(float(obj['current']['pos'][axis]) for axis in ('x','y','z'))
            if self.inside == 60:
                self.origin = xyz
                print('[room] house loaded, player position=%s' % (xyz,))
            gdb.selected_inferior().write_memory(keybase + 26, bytes([60 <= self.inside < 90]))
            if self.inside >= 100 and self.origin:
                distance = sum((xyz[i]-self.origin[i])**2 for i in (0,2))**0.5
                print('[room] house movement distance=%.3f' % distance)
                if distance > 10:
                    print('[room] SUCCESS: diagnostic transition and controllable house interior')
                    gdb.set_convenience_variable('tww_probe_success', 1)
                    return True
        return False
RoomTransition()
end
source ../tools/crash.gdb
