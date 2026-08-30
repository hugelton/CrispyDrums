#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace CrispyDrums {

constexpr float kTwoPi = 6.2831853071795864769f;

enum class Wave : uint8_t {
  Saw,
  Square,
  Pulse,
  DoubleSine,
  SawPulse,
  RezSaw,
  RezTri,
  RezPulse
};

enum class PhaseMap : uint8_t {
  None,
  Grain,
  Metal,
  Skin,
  Crackle
};

inline float wrap01(float x) { return x - std::floor(x); }
inline float decay(float t, float tau) { return std::exp(-t / std::max(tau, 1.0e-5f)); }

// Deterministic, periodic phase maps. They never enter the output as amplitude noise.
// The apparent noise is created by irregular phase readout of the sine carrier.
inline float phaseMap(float phase, PhaseMap map) {
  switch (map) {
    case PhaseMap::None: return 0.0f;
    case PhaseMap::Metal:
      return 0.50f * std::sin(kTwoPi * 7.0f * phase)
           + 0.31f * std::sin(kTwoPi * 11.0f * phase + 0.4f)
           + 0.19f * std::sin(kTwoPi * 17.0f * phase + 1.1f);
    case PhaseMap::Skin:
      return 0.55f * std::sin(kTwoPi * 2.0f * phase + 0.2f)
           + 0.28f * std::sin(kTwoPi * 3.0f * phase + 1.4f)
           + 0.12f * std::sin(kTwoPi * 5.0f * phase + 0.7f)
           + 0.05f * std::sin(kTwoPi * 8.0f * phase + 2.2f);
    case PhaseMap::Crackle: {
      const float a = std::sin(kTwoPi * 29.0f * phase);
      const float b = std::sin(kTwoPi * 43.0f * phase + 0.7f);
      return (a * b > 0.62f) ? (a * b) : 0.0f;
    }
    case PhaseMap::Grain: {
      // A 64-segment fixed pseudo-random phase map. It repeats every oscillator
      // cycle, so it remains a pitched PD oscillator rather than white noise.
      uint32_t i = static_cast<uint32_t>(phase * 64.0f) & 63u;
      uint32_t x = i + 0x9e3779b9u;
      x ^= x >> 16; x *= 0x7feb352du;
      x ^= x >> 15; x *= 0x846ca68bu;
      x ^= x >> 16;
      return (static_cast<float>(x & 0xffffu) / 32767.5f) - 1.0f;
    }
  }
  return 0.0f;
}

// Normalized versions of the CrispyZebra/CZ phase-distortion shapes.
inline float warp(float p, float dcw, Wave w) {
  dcw = std::clamp(dcw, 0.0f, 1.0f);
  switch (w) {
    case Wave::Saw: {
      const float xp = 0.5f + 0.49998f * dcw;
      if (p < xp) return 0.5f * p / std::max(xp, 1.0e-6f);
      return 0.5f + 0.5f * (p - xp) / std::max(1.0f - xp, 1.0e-6f);
    }
    case Wave::Square: {
      const float xp = 0.5f - 0.47486f * dcw;
      if (p < xp) return 0.5f * p / std::max(xp, 1.0e-6f);
      if (p < 0.5f) return 0.5f;
      if (p < 0.5f + xp) return 0.5f + 0.5f * (p - 0.5f) / std::max(xp, 1.0e-6f);
      return 1.0f;
    }
    case Wave::Pulse: {
      const float xp = 1.0f - 0.96873f * dcw;
      return p < xp ? p / std::max(xp, 1.0e-6f) : 1.0f;
    }
    case Wave::DoubleSine: {
      const float xp = 1.0f - 0.74999f * dcw;
      if (p < xp) return p / std::max(xp, 1.0e-6f);
      return (p - xp) / std::max(1.0f - xp, 1.0e-6f);
    }
    case Wave::SawPulse: {
      const float xp = 1.0f - 0.49999f * dcw;
      const float peak = xp * (0.5f + 0.45f * dcw);
      if (p >= xp) return 1.0f;
      if (p < peak) return 0.5f * p / std::max(peak, 1.0e-6f);
      return 0.5f + 0.5f * (p - peak) / std::max(xp - peak, 1.0e-6f);
    }
    case Wave::RezSaw:
    case Wave::RezTri:
    case Wave::RezPulse: {
      // Resonance family: DCW accelerates phase within each base cycle.
      return wrap01(p + p * dcw * 4.70f);
    }
  }
  return p;
}

inline float oscillator(float& phase, float hz, float sr, float dcw, Wave wave,
                        PhaseMap map = PhaseMap::None, float mapAmount = 0.0f,
                        float mapOffset = 0.0f) {
  phase = wrap01(phase + hz / sr);
  float q = warp(phase, dcw, wave);
  if (map != PhaseMap::None && mapAmount != 0.0f)
    q = wrap01(q + phaseMap(wrap01(phase + mapOffset), map) * mapAmount * dcw);

  float y = std::sin(kTwoPi * q);
  if (wave == Wave::RezSaw) y *= (1.0f - phase);
  else if (wave == Wave::RezTri) y *= (phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f);
  else if (wave == Wave::RezPulse) y *= (phase < 0.5f ? 1.0f : 0.0f);
  return y;
}

inline float voiceDuration(int v) {
  static constexpr float d[8] = {.55f,.34f,.43f,.34f,.16f,.62f,.42f,.20f};
  return d[std::clamp(v,0,7)];
}

inline float renderVoice(int v, float t, float sr, float* ph) {
  switch (v) {
    case 0: { // Kick: strong DCW transient collapsing toward a near-sine body.
      const float f = 47.0f + 118.0f * decay(t,.018f);
      const float dcw = .025f + .62f * decay(t,.028f);
      const float body = oscillator(ph[0],f,sr,dcw,Wave::SawPulse);
      const float edge = oscillator(ph[1],f*2.01f,sr,.74f*decay(t,.010f),Wave::RezSaw);
      return (body + .11f*edge*decay(t,.012f)) * decay(t,.205f);
    }
    case 1: { // Snare: no white noise. Grain is a repeating irregular phase map.
      const float body = oscillator(ph[0],178.0f+18.0f*decay(t,.010f),sr,
                                    .12f+.34f*decay(t,.020f),Wave::RezTri,
                                    PhaseMap::Skin,.055f) * decay(t,.070f);
      const float g1 = oscillator(ph[1],1289.0f,sr,.94f,Wave::DoubleSine,
                                  PhaseMap::Grain,.41f);
      const float g2 = oscillator(ph[2],1973.0f,sr,.91f,Wave::Pulse,
                                  PhaseMap::Grain,.34f,.37f);
      const float crack = oscillator(ph[3],3119.0f,sr,.98f,Wave::RezPulse,
                                     PhaseMap::Crackle,.30f,.19f);
      return .28f*body + (.46f*g1+.34f*g2)*decay(t,.105f) + .19f*crack*decay(t,.016f);
    }
    case 2:
    case 3: { // Toms: membrane-like resonant PD, deliberately little pitch dive.
      const bool hi=v==3;
      const float base=hi?164.0f:104.0f;
      const float f=base+(hi?10.0f:7.0f)*decay(t,.014f);
      const float dcw=.06f+.28f*decay(t,.032f);
      const float a=oscillator(ph[0],f,sr,dcw,Wave::RezTri,PhaseMap::Skin,.045f);
      const float b=oscillator(ph[1],f*1.53f,sr,.18f+.15f*decay(t,.025f),Wave::RezSaw,PhaseMap::Skin,.025f,.2f);
      const float c=oscillator(ph[2],f*2.08f,sr,.14f,Wave::Saw,PhaseMap::Skin,.018f,.5f);
      return (.80f*a+.14f*b+.06f*c)*decay(t,hi?.17f:.225f);
    }
    case 4:
    case 5: { // Hats: inharmonic PD bank, irregular phase maps, no noise source.
      static constexpr float fs[6]={2167,2783,3469,4231,5279,6421};
      float y=0.0f;
      for(int i=0;i<6;i++) {
        const Wave w=(i&1)?Wave::Pulse:Wave::DoubleSine;
        const PhaseMap m=(i%3==0)?PhaseMap::Grain:PhaseMap::Metal;
        y += oscillator(ph[i],fs[i],sr,.94f,w,m,(i%3==0)?.30f:.13f,.13f*i);
      }
      y/=6.0f;
      const float tail=v==4?decay(t,.039f):(.72f*decay(t,.215f)+.28f*decay(t,.052f));
      return y*tail;
    }
    case 6: { // Cowbell: two dominant inharmonic PD partials plus a weak upper mode.
      const float a=oscillator(ph[0],562.0f,sr,.76f,Wave::Square,PhaseMap::Metal,.025f);
      const float b=oscillator(ph[1],845.0f,sr,.82f,Wave::SawPulse,PhaseMap::Metal,.020f,.27f);
      const float c=oscillator(ph[2],1198.0f,sr,.44f,Wave::RezSaw);
      return (.60f*a+.33f*b+.07f*c)*decay(t,.19f);
    }
    case 7: { // Rimshot: very short clustered resonances + phase discontinuity click.
      const float a=oscillator(ph[0],1647.0f,sr,.52f,Wave::RezPulse);
      const float b=oscillator(ph[1],2381.0f,sr,.61f,Wave::RezSaw,PhaseMap::Crackle,.08f,.21f);
      const float c=oscillator(ph[2],3319.0f,sr,.41f,Wave::DoubleSine,PhaseMap::Grain,.12f,.61f);
      return (.50f*a+.32f*b+.18f*c)*decay(t,.030f);
    }
  }
  return 0.0f;
}

inline int renderHit(int voice, float* out, int maxFrames, int sampleRate) {
  if(!out || maxFrames<=0 || sampleRate<=0) return 0;
  voice=std::clamp(voice,0,7);
  const int n=std::min(maxFrames,static_cast<int>(std::ceil(voiceDuration(voice)*sampleRate)));
  float ph[8]={};
  float peak=1.0e-9f;
  for(int i=0;i<n;i++) {
    const float t=static_cast<float>(i)/sampleRate;
    out[i]=renderVoice(voice,t,static_cast<float>(sampleRate),ph);
    peak=std::max(peak,std::abs(out[i]));
  }
  const float gain=.88f/peak;
  for(int i=0;i<n;i++) out[i]=std::clamp(out[i]*gain,-1.0f,1.0f);
  return n;
}

} // namespace CrispyDrums
