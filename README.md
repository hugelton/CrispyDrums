# CrispyDrums

Experimental 8-track Phase Distortion drum synthesizer derived from the CrispyZebra synthesis architecture.

## Prototype goal

- 8 identical drum voices; instrument roles are patches, not separate engines.
- No drum samples.
- DCO envelope becomes a compact drum pitch envelope.
- DCW envelope becomes the main transient/timbre envelope.
- DCA envelope controls amplitude.
- Drum-oriented phase tables add deterministic Noise / Metal / Crackle / Skin-style phase displacement before the sine lookup rather than mixing ordinary noise at the output.
- Current first-pass instruments: Kick, Snare, Tom L, Tom H, Clap, Hat, Metal, Perc.

The production direction is to reuse the CrispyZebra fixed-point PD core, but replace the vintage 8-stage editor with a smaller drum-oriented envelope model.

## Audition prototype

`prototype/render.py` renders a 2-bar WAV for quick DSP experiments.

```sh
python3 prototype/render.py
```

The generated file is `prototype/CrispyDrums_prototype.wav`.

### Tom experiment

The first tom attempt sounded too much like a conventional pitch-swept synth. The current prototype deliberately uses:

- much shallower pitch drop,
- shorter decay,
- low-order irregular `SKIN` phase displacement,
- a quiet second inharmonic membrane mode around 1.47x,
- DCW texture concentrated near the transient.

This is still synthetic, but the target is a compact electronic drum-body sound rather than a laser/tom synth patch.

## Status

Early DSP sketch only. No plugin UI or sequencer yet.
