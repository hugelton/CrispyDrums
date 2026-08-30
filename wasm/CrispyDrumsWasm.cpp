#include <cmath>
#include <cstdint>
#include <algorithm>
#include <emscripten/emscripten.h>

namespace {
constexpr float PI2=6.2831853071795864769f;
inline float wrap(float x){return x-std::floor(x);} 
inline float env(float t,float tau){return std::exp(-t/std::max(tau,1.0e-5f));}
inline float hashNoise(float x){float v=std::sin((x*65536.0f+17.0f)*12.9898f)*43758.5453f;return (v-std::floor(v))*2.0f-1.0f;}
inline float pdSaw(float p,float a){float xp=std::clamp(0.5f+0.49f*a,0.501f,0.995f);return p<xp?0.5f*p/xp:0.5f+0.5f*(p-xp)/(1.0f-xp);}
inline float osc(float& phase,float hz,float sr,float dcw,float noiseAmt=0.0f,float phaseOff=0.0f){phase=wrap(phase+hz/sr);float q=pdSaw(phase,dcw);if(noiseAmt>0)q=wrap(q+hashNoise(phase+phaseOff)*noiseAmt);return std::sin(PI2*q);} 

float renderVoice(int v,float t,float sr,float* ph){
  switch(v){
    case 0:{float f=48.0f+120.0f*env(t,.020f);float y=osc(ph[0],f,sr,.03f+.34f*env(t,.025f));float c=osc(ph[1],2200,sr,.8f,.06f)*env(t,.004f);return (y+.10f*c)*env(t,.20f);} 
    case 1:{float body=osc(ph[0],185+35*env(t,.012f),sr,.12f+.18f*env(t,.018f))*env(t,.065f);float n1=osc(ph[1],1379,sr,.88f,.46f);float n2=osc(ph[2],2137,sr,.94f,.39f,.27f);float n=.58f*n1+.42f*n2;return .24f*body+.78f*n*env(t,.115f)+.18f*(n1*n2)*env(t,.018f);} 
    case 2: case 3:{bool hi=v==3;float base=hi?165.0f:105.0f;float drop=hi?12.0f:9.0f;float f=base+drop*env(t,.016f);float a=osc(ph[0],f,sr,.055f+.12f*env(t,.025f));float b=osc(ph[1],f*1.53f,sr,.08f+.05f*env(t,.02f));float c=osc(ph[2],f*2.08f,sr,.06f+.04f*env(t,.018f));float skin=osc(ph[3],1733,sr,.85f,.32f)*env(t,.008f);return (.78f*a+.14f*b+.08f*c)*env(t,hi?.18f:.24f)+.13f*skin;} 
    case 4: case 5:{static const float fs[6]={2213,2831,3511,4217,5147,6311};float y=0;for(int i=0;i<6;i++)y+=osc(ph[i],fs[i],sr,.96f,.26f,.11f*i);y/=6.0f;float tail=v==4?env(t,.042f):(.72f*env(t,.22f)+.28f*env(t,.055f));return y*tail;} 
    case 6:{float a=osc(ph[0],562,sr,.72f);float b=osc(ph[1],845,sr,.78f,0,.23f);float c=osc(ph[2],1198,sr,.42f,0,.47f);float impact=osc(ph[3],2670,sr,.92f,.18f)*env(t,.006f);return (.58f*a+.34f*b+.08f*c)*env(t,.19f)+.10f*impact;} 
    case 7:{float a=osc(ph[0],1680,sr,.48f);float b=osc(ph[1],2447,sr,.57f,0,.31f);float c=osc(ph[2],3271,sr,.35f,0,.63f);float click=osc(ph[3],5100,sr,.94f,.28f,.17f)*env(t,.0025f);return ((.48f*a+.34f*b+.18f*c)*env(t,.028f)+.24f*click)*env(t,.045f);} 
  }
  return 0;
}

float voiceDur(int v){static const float d[8]={.55f,.34f,.43f,.34f,.16f,.62f,.42f,.20f};return d[std::clamp(v,0,7)];}
}

extern "C" EMSCRIPTEN_KEEPALIVE
int crispy_render_hit(int voice,float* out,int maxFrames,int sampleRate){
  if(!out||maxFrames<=0||sampleRate<=0)return 0;
  voice=std::clamp(voice,0,7);
  int n=std::min(maxFrames,(int)std::ceil(voiceDur(voice)*sampleRate));
  float ph[8]={};float peak=1.0e-9f;
  for(int i=0;i<n;i++){float t=(float)i/sampleRate;out[i]=renderVoice(voice,t,(float)sampleRate,ph);peak=std::max(peak,std::abs(out[i]));}
  float g=.88f/peak;for(int i=0;i<n;i++)out[i]=std::clamp(out[i]*g,-1.0f,1.0f);
  return n;
}
