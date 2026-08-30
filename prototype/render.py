#!/usr/bin/env python3
import math
import wave
import numpy as np

SR = 44100
BPM = 116
STEP = 60.0 / BPM / 4.0
STEPS = 32
N = int((STEPS * STEP + 1.0) * SR)
MIX = np.zeros((N, 2), dtype=np.float64)


def pd_saw(ph, dcw):
    xp = 0.5 + 0.49 * dcw
    return np.where(ph < xp,
                    0.5 * ph / np.maximum(xp, 1e-6),
                    0.5 + 0.5 * (ph - xp) / np.maximum(1.0 - xp, 1e-6))


def pd_double(ph, dcw):
    xp = np.maximum(0.04, 1.0 - 0.75 * dcw)
    return np.where(ph < xp,
                    ph / np.maximum(xp, 1e-6),
                    (ph - xp) / np.maximum(1.0 - xp, 1e-6)) % 1.0


def fixed_table(kind, size=2048, seed=1):
    x = np.arange(size) / size
    rng = np.random.default_rng(seed)
    if kind == "noise":
        z = rng.uniform(-1, 1, size)
        return np.convolve(z, np.ones(9) / 9.0, mode="same")
    if kind == "metal":
        return (0.50 * np.sin(2*np.pi*7*x)
                + 0.31 * np.sin(2*np.pi*11*x + 0.4)
                + 0.19 * np.sin(2*np.pi*17*x + 1.1))
    if kind == "skin":
        return (0.55 * np.sin(2*np.pi*2*x + 0.2)
                + 0.28 * np.sin(2*np.pi*3*x + 1.4)
                + 0.12 * np.sin(2*np.pi*5*x + 0.7)
                + 0.05 * np.sin(2*np.pi*8*x + 2.2))
    if kind == "crackle":
        z = np.zeros(size)
        idx = rng.choice(size, 64, replace=False)
        z[idx] = rng.uniform(-1, 1, len(idx))
        return np.convolve(z, np.array([0.15, 0.7, 0.15]), mode="same")
    return np.zeros(size)


TABLE = {
    "noise": fixed_table("noise", seed=10),
    "metal": fixed_table("metal", seed=11),
    "skin": fixed_table("skin", seed=12),
    "crackle": fixed_table("crackle", seed=13),
}


def voice(freq, dur, pitch_drop, amp_decay, dcw0, dcw1, dcw_decay,
          warp="saw", table=None, table_amt=0.0, attack=0.0015,
          drive=1.0, membrane=False):
    n = int(dur * SR)
    t = np.arange(n) / SR
    f = freq + pitch_drop * np.exp(-t / max(0.018, dcw_decay * 0.65))
    ph = np.cumsum(f / SR) % 1.0
    dcw = dcw1 + (dcw0 - dcw1) * np.exp(-t / max(dcw_decay, 1e-4))
    pd = pd_double(ph, dcw) if warp == "double" else pd_saw(ph, dcw)

    if table:
        tab = TABLE[table]
        idx = (ph * len(tab)).astype(int) % len(tab)
        pd = (pd + tab[idx] * table_amt * dcw) % 1.0

    y = np.sin(2*np.pi*pd)

    if membrane:
        ph2 = np.cumsum((f * 1.47) / SR) % 1.0
        dcw2 = 0.14 + 0.22 * np.exp(-t / 0.055)
        pd2 = pd_saw(ph2, dcw2)
        idx2 = (ph2 * len(TABLE["skin"])).astype(int) % len(TABLE["skin"])
        pd2 = (pd2 + TABLE["skin"][idx2] * 0.055 * dcw2) % 1.0
        y = 0.82 * y + 0.18 * np.sin(2*np.pi*pd2)

    amp = np.where(t < attack,
                   t / max(attack, 1/SR),
                   np.exp(-(t - attack) / amp_decay))
    amp *= np.exp(-0.22 * (t / max(amp_decay, 0.001))**2)
    y *= amp
    return np.tanh(y * drive) / max(np.tanh(drive), 1e-6)


def add(step, y, pan=0.0, gain=0.5):
    start = int(step * STEP * SR)
    end = min(N, start + len(y))
    y = y[:end-start] * gain
    MIX[start:end, 0] += y * math.sqrt((1-pan)/2)
    MIX[start:end, 1] += y * math.sqrt((1+pan)/2)


def kick():  return voice(47, .62, 132, .30, .68, .015, .052, drive=1.5)
def snare(): return voice(178, .34, 20, .15, .94, .18, .060, "double", "noise", .24, .001, 1.8)
def tom_l(): return voice(103, .42, 23, .235, .46, .085, .058, "saw", "skin", .075, .0012, 1.35, True)
def tom_h(): return voice(158, .34, 31, .180, .43, .075, .050, "saw", "skin", .068, .0012, 1.30, True)
def clap():  return voice(310, .25, 0, .095, .95, .35, .028, "double", "crackle", .24, .0005, 2.0)
def hat():   return voice(930, .13, 0, .045, .98, .50, .020, "double", "noise", .31, .0004, 1.5)
def metal(): return voice(515, .38, 8, .22, .92, .35, .090, "double", "metal", .18, .001, 1.6)
def perc():  return voice(270, .30, 48, .15, .70, .08, .052, "saw", "metal", .08, .001, 1.4)


for s in [0, 8, 16, 24, 28]: add(s, kick(), 0, .88)
for s in [4, 12, 20, 28]: add(s, snare(), 0, .50)
for s in range(0, 32, 2): add(s, hat(), .25 if s % 4 else -.18, .17)
for s in [7, 15, 23, 31]: add(s, clap(), .10, .24)
for s in [10, 26]: add(s, tom_l(), -.30, .55)
for s in [11, 27]: add(s, tom_h(), .30, .50)
for s in [6, 14, 22, 30]: add(s, metal(), .45, .20)
for s in [3, 19, 25]: add(s, perc(), -.45, .25)

MIX *= .92 / max(np.max(np.abs(MIX)), 1e-9)
with wave.open("prototype/CrispyDrums_prototype.wav", "wb") as wf:
    wf.setnchannels(2)
    wf.setsampwidth(2)
    wf.setframerate(SR)
    wf.writeframes((np.clip(MIX, -1, 1) * 32767).astype("<i2").tobytes())

print("prototype/CrispyDrums_prototype.wav")
