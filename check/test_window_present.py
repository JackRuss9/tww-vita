#!/usr/bin/env python3
"""Run the production presentation function with a queued-draw ordering check."""
import argparse
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--source', type=pathlib.Path, default=root / 'sms-port/platform/gx/src/gx_platform.cpp')
args = parser.parse_args()
source = args.source.read_text()
start = source.index('void GXPC_Present(const void* xfb)')
end = source.index('\nvoid sms_gx_set_event_callback', start)
function = source[start:end]
harness = r'''
#include <algorithm>
#include <stdexcept>
#include <cstdio>
#define SMS_GX_HAVE_SDL2
static int target=1, swaps=0, restored=0;
static bool hdr=false, minimized=false;
static int MODE_WINDOW=1, s_mode=1, s_frame=0, s_vsync=0;
static void* s_window=(void*)1;
static double g_presentWaited=0,g_presentDrain=0,g_presentSeconds=0,g_swapSeconds=0,s_lastPresent=0,s_presentCost=0;
static double nowSeconds(){static double t=0;return t+=.001;}
static void SDL_GL_GetDrawableSize(void*,int*w,int*h){*w=960;*h=720;}
static bool hdrActive(){return hdr;}
static bool hdrFrameBegin(int,int){return !minimized;}
static void swap(){if(target!=0)throw std::runtime_error("EFB restored before swap");++swaps;}
static void hdrFramePresent(bool){swap();}
static void SDL_GL_SwapWindow(void*){swap();}
static void GXPC_PresentXFB(const void*,int,int){target=0;}
static void GXPC_EndPresent(){target=1;++restored;}
static void GXPC_OverlayDraw(int,int){}
static void cheatMenuRender(int,int){}
static void warpMenuRender(int,int){}
static void quitPromptRender(int,int){}
static void savestateOverlayRender(int,int){}
static void extOverlayRender(int,int){}
static void captureWindow(int,int,int){}
static void waitSimulatedRefresh(){}
static void sms_gx_pump_events(){}
namespace glt {
static double waitedSeconds(){return 0;}
static void finish(){}
template<class F> void sync(F callback,bool serviceGuest=true){
 if(serviceGuest)throw std::runtime_error("presentation allows reentrant guest rendering");
 callback();
 // The next queued guest draw can execute immediately after presentation,
 // before the waiting caller resumes. It must already target the EFB.
 if(target!=1)throw std::runtime_error("queued guest draw inherits window framebuffer");
}
}
'''
main = r'''
int main(){try {
 GXPC_Present((void*)1);
 hdr=true; GXPC_Present((void*)1);
 minimized=true; GXPC_Present((void*)1);
 if(swaps!=2 || restored!=2)throw std::runtime_error("wrong normal/HDR/minimized handling");
 std::puts("PASS: EFB restored before queued guest draws; SDL/HDR swaps retain window target");
 return 0;
}catch(const std::exception&e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
'''
with tempfile.TemporaryDirectory(prefix='tww-present-test-') as tmp:
    cpp = pathlib.Path(tmp) / 'present.cpp'
    binary = pathlib.Path(tmp) / 'present'
    cpp.write_text(harness + function + main)
    subprocess.run(['g++', '-std=c++11', '-O2', str(cpp), '-o', str(binary)], check=True)
    result = subprocess.run([str(binary)])
    raise SystemExit(result.returncode)
