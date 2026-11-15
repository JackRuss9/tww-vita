# Stop at the first missing device samples and show what held the game CPU.
# This is diagnosis only; measure final audio without a debugger.
set pagination off
set confirm off
set debuginfod enabled off
handle SIGUSR1 SIGUSR2 SIGPIPE nostop noprint
break sms-port/platform/audio/ai.cpp:206
commands
silent
printf "[audio-probe] first output underrun\n"
thread apply all bt 18
quit 0
end
run
