# Diagnostic replay of savedata earned by the real sword training events.
# Not a normal menu/door playthrough; no invented quest flags or item grants.
set pagination off
set confirm off
set python print-stack full
handle SIGUSR1 SIGUSR2 SIGPIPE nostop noprint
python
import gdb,sys,os
sys.dont_write_bytecode=True
sys.path.insert(0,os.path.abspath('../tools'))
from forest_story import ForestStory
assert os.environ.get('TWW_FOREST_CHECKPOINT'), 'Provide TWW_FOREST_CHECKPOINT from the earned sword state'
keybase=int(gdb.parse_and_eval("&'(anonymous namespace)::g_key'"))
class Resume(gdb.Breakpoint):
    def __init__(self):
        super().__init__('daPy_lk_c::execute')
        self.age=0;self.done=False
    def stop(self):
        if self.done:return False
        self.age+=1
        if self.age==180:
            saved=gdb.parse_and_eval('g_dComIfG_gameInfo.save.mSavedata')
            raw=open(os.environ['TWW_FOREST_CHECKPOINT'],'rb').read()
            assert len(raw)==saved.type.sizeof
            gdb.selected_inferior().write_memory(int(saved.address),raw)
            self.done=True
            print('[forest-replay] restored explicitly supplied native savedata; diagnostic stage request follows')
            ForestStory(keybase)
        return False
Resume()
end
source ../tools/crash.gdb

python
import orca_story,importlib
importlib.reload(orca_story)
orca_story.describe()
end

python
import forest_story,importlib
importlib.reload(forest_story)
forest_story.describe()
end
