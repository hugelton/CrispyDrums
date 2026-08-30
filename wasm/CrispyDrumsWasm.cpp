#include <emscripten/emscripten.h>
#include "../src/CrispyDrumsCore.h"

extern "C" EMSCRIPTEN_KEEPALIVE
int crispy_render_hit(int voice,float* out,int maxFrames,int sampleRate){
  return CrispyDrums::renderHit(voice,out,maxFrames,sampleRate);
}
