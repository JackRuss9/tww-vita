# Use with tools/probe-pc.sh --outset --gdb "$PWD/tools/movement.gdb".
# Sample the actual player, including idle, input, movement and ground height.
set pagination off
set confirm off
handle SIGUSR1 SIGUSR2 SIGPIPE nostop noprint
set $player_frames = 0
break daPy_lk_c::execute
commands
silent
set $player_frames = $player_frames + 1
if $player_frames % 60 == 1
printf "[movement] frame=%d pos=(%.3f,%.3f,%.3f) ground=%.3f proc=%d stick=%.3f speed=%.3f demo=%d/%d room=%d\n", $player_frames, this->current.pos.x, this->current.pos.y, this->current.pos.z, this->mAcch.m_ground_h, this->mCurProc, this->mStickDistance, this->speedF, this->mDemo.mDemoType, this->mDemo.mDemoMode, this->current.roomNo
end
continue
end
break daPy_lk_c::procFrontRoll_init
commands
silent
printf "[movement] front roll requested\n"
continue
end
source ../tools/crash.gdb
