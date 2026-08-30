#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace CrispyDrums {

enum class PhaseTable : uint8_t {
  Classic = 0,
  Noise,
  Metal,
  Crackle,
  Skin
};

struct ADEnvelope {
  float attack_s = 0.001f;
  float decay_s = 0.2f;
  float value = 0.0f;
  bool active = false;
  bool attacking = false;

  void trigger() {
    value = 0.0f;
    active = true;
    attacking = true;
  }

  float next(float sample_rate) {
    if (!active) return 0.0f;
    if (attacking) {
      const float step = 1.0f / (attack_s * sample_rate + 1.0f);
      value += step;
      if (value >= 1.0f) {
        value = 1.0f;
        attacking = false;
      }
    } else {
      const float c = std::exp(-1.0f / (decay_s * sample_rate + 1.0f));
      value *= c;
      if (value < 1.0e-5f) {
        value = 0.0f;
        active = false;
      }
    }
    return value;
  }
};

struct DrumPatch {
  float base_hz = 100.0f;
  float pitch_drop_hz = 20.0f;
  float pitch_decay_s = 0.05f;

  float dcw_start = 0.5f;
  float dcw_end = 0.05f;
  float dcw_decay_s = 0.07f;

  float amp_attack_s = 0.001f;
  float amp_decay_s = 0.2f;

  PhaseTable phase_table = PhaseTable::Classic;
  float phase_table_amount = 0.0f;

  float second_mode_mix = 0.0f;
  float second_mode_ratio = 1.47f;
};

// Production TODO: replace floating-point helpers with the CrispyZebra Q16
// arithmetic and sine LUT once the drum topology is settled.
class Voice {
public:
  void setup(float sample_rate) { sample_rate_ = sample_rate; }

  void setPatch(const DrumPatch& patch) { patch_ = patch; }

  void trigger() {
    phase1_ = 0.0f;
    phase2_ = 0.0f;
    elapsed_ = 0.0f;
    amp_.attack_s = patch_.amp_attack_s;
    amp_.decay_s = patch_.amp_decay_s;
    amp_.trigger();
  }

  float next() {
    if (!amp_.active) return 0.0f;

    const float pitch_env = std::exp(-elapsed_ / (patch_.pitch_decay_s + 1.0e-6f));
    const float hz = patch_.base_hz + patch_.pitch_drop_hz * pitch_env;
    const float dcw_env = std::exp(-elapsed_ / (patch_.dcw_decay_s + 1.0e-6f));
    const float dcw = patch_.dcw_end + (patch_.dcw_start - patch_.dcw_end) * dcw_env;

    phase1_ += hz / sample_rate_;
    phase1_ -= std::floor(phase1_);
    phase2_ += (hz * patch_.second_mode_ratio) / sample_rate_;
    phase2_ -= std::floor(phase2_);

    float pd1 = warpSaw(phase1_, dcw);
    pd1 = wrap01(pd1 + phaseOffset(phase1_, patch_.phase_table) * patch_.phase_table_amount * dcw);
    float out = std::sin(kTwoPi * pd1);

    if (patch_.second_mode_mix > 0.0f) {
      const float secondary_dcw = 0.14f + 0.22f * std::exp(-elapsed_ / 0.055f);
      float pd2 = warpSaw(phase2_, secondary_dcw);
      pd2 = wrap01(pd2 + phaseOffset(phase2_, PhaseTable::Skin) * 0.055f * secondary_dcw);
      const float secondary = std::sin(kTwoPi * pd2);
      out = out * (1.0f - patch_.second_mode_mix) + secondary * patch_.second_mode_mix;
    }

    elapsed_ += 1.0f / sample_rate_;
    return out * amp_.next(sample_rate_);
  }

private:
  static constexpr float kTwoPi = 6.2831853071795864769f;

  static float wrap01(float x) {
    x -= std::floor(x);
    return x;
  }

  static float warpSaw(float phase, float dcw) {
    const float xp = 0.5f + 0.49f * dcw;
    if (phase < xp) return 0.5f * phase / (xp + 1.0e-6f);
    return 0.5f + 0.5f * (phase - xp) / (1.0f - xp + 1.0e-6f);
  }

  // Deterministic phase displacement. These are placeholders for actual
  // fixed tables. The key rule is that noise-like energy enters the PHASE
  // mapping, not as a post-oscillator noise mixer.
  static float phaseOffset(float phase, PhaseTable table) {
    switch (table) {
      case PhaseTable::Classic:
        return 0.0f;
      case PhaseTable::Metal:
        return 0.50f * std::sin(kTwoPi * 7.0f * phase)
             + 0.31f * std::sin(kTwoPi * 11.0f * phase + 0.4f)
             + 0.19f * std::sin(kTwoPi * 17.0f * phase + 1.1f);
      case PhaseTable::Skin:
        return 0.55f * std::sin(kTwoPi * 2.0f * phase + 0.2f)
             + 0.28f * std::sin(kTwoPi * 3.0f * phase + 1.4f)
             + 0.12f * std::sin(kTwoPi * 5.0f * phase + 0.7f)
             + 0.05f * std::sin(kTwoPi * 8.0f * phase + 2.2f);
      case PhaseTable::Crackle:
        return std::sin(kTwoPi * 31.0f * phase) * std::sin(kTwoPi * 47.0f * phase);
      case PhaseTable::Noise: {
        const float x = std::sin((phase * 65536.0f + 17.0f) * 12.9898f) * 43758.5453f;
        return (x - std::floor(x)) * 2.0f - 1.0f;
      }
    }
    return 0.0f;
  }

  float sample_rate_ = 44100.0f;
  DrumPatch patch_{};
  ADEnvelope amp_{};
  float phase1_ = 0.0f;
  float phase2_ = 0.0f;
  float elapsed_ = 0.0f;
};

inline DrumPatch TomLowPrototype() {
  DrumPatch p;
  p.base_hz = 103.0f;
  p.pitch_drop_hz = 23.0f;
  p.pitch_decay_s = 0.038f;
  p.dcw_start = 0.46f;
  p.dcw_end = 0.085f;
  p.dcw_decay_s = 0.058f;
  p.amp_decay_s = 0.235f;
  p.phase_table = PhaseTable::Skin;
  p.phase_table_amount = 0.075f;
  p.second_mode_mix = 0.18f;
  return p;
}

inline DrumPatch TomHighPrototype() {
  DrumPatch p = TomLowPrototype();
  p.base_hz = 158.0f;
  p.pitch_drop_hz = 31.0f;
  p.pitch_decay_s = 0.032f;
  p.dcw_start = 0.43f;
  p.dcw_end = 0.075f;
  p.dcw_decay_s = 0.050f;
  p.amp_decay_s = 0.180f;
  p.phase_table_amount = 0.068f;
  return p;
}

} // namespace CrispyDrums
