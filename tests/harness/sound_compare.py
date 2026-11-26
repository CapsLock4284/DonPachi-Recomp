#!/usr/bin/env python3
"""sound_compare.py: the port's sound against the real MAME's, from tools/mame/dp_check.lua's run.

    python3 tests/harness/sound_compare.py DIR [SECONDS]

DIR holds sound.wav (MAME's -wavwrite) and port.wav (tests/harness/mame_check with SOUND=1, the same inputs); SECONDS:
how far the two games stay together (mame_check prints where they part: its VBLANK / 57.55), the end by default.
Lines the two up (MAME's starts with its own power-on, a little before the port's first frame), then, second by
second while the games are together: how loud each is (RMS, in dB below full scale) and the difference, how alike
their loudness moves (the envelopes' correlation), how alike the waves themselves are (their correlation at the
best offset in that second: the same samples played the same way come out near 1), and the balance of low, middle
and high frequencies (from 20 Hz: MAME's -wavwrite keeps the OKIs' DC, the port's speaker filter takes it out).
Prints a summary and the seconds that differ most; writes DIR/compare.wav, the first minute
with MAME's sound on the left and the port's on the right, to listen to side by side.
Needs numpy.
"""
import sys
import wave

import numpy as np


def read_wav(path):
    with wave.open(path, "rb") as w:
        channels, width, rate, frames = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(frames)
    if width != 2:
        sys.exit(f"{path}: {8 * width}-bit samples: only 16-bit read")
    data = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
    if channels > 1:
        data = data.reshape(-1, channels).mean(axis=1)
    return data, rate


def resample(data, rate, to):
    if rate == to:
        return data
    count = int(len(data) * to / rate)
    return np.interp(np.arange(count) * rate / to, np.arange(len(data)), data)


def envelope(data, rate, step=0.01):
    n = int(rate * step)
    usable = len(data) // n * n
    return np.sqrt((data[:usable].reshape(-1, n) ** 2).mean(axis=1) + 1e-12)


def without_lows(data, rate, below=20.0):
    """the second's frequencies under 20 Hz taken out: MAME's -wavwrite keeps the OKIs' DC (it wanders up to a fifth
    of full scale), which the port's speaker filter (MAME's own 20 Hz high-pass) takes out; inaudible either way"""
    spectrum = np.fft.rfft(data)
    spectrum[np.fft.rfftfreq(len(data), 1.0 / rate) < below] = 0
    return np.fft.irfft(spectrum, len(data))


def db(x):
    return 20.0 * np.log10(max(x, 1e-9))


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    folder = sys.argv[1]
    mame, mame_rate = read_wav(folder + "/sound.wav")
    port, port_rate = read_wav(folder + "/port.wav")
    rate = 48000
    mame, port = resample(mame, mame_rate, rate), resample(port, port_rate, rate)
    print(f"MAME's: {len(mame) / rate:.1f} s ({mame_rate} Hz), the port's: {len(port) / rate:.1f} s ({port_rate} Hz)")

    # lined up: the envelopes over the first 3 minutes (10 ms steps), then the samples near that
    e_mame, e_port = envelope(mame[: rate * 180], rate), envelope(port[: rate * 180], rate)
    n = min(len(e_mame), len(e_port))
    best, lag = -1.0, 0
    for shift in range(-500, 501):
        a = e_mame[max(0, shift): n + min(0, shift)]
        b = e_port[max(0, -shift): n - max(0, shift)]
        m = min(len(a), len(b))
        if m < 1000:
            continue
        c = np.corrcoef(a[:m], b[:m])[0, 1]
        if c > best:
            best, lag = c, shift
    coarse = lag * rate // 100
    window = rate * 20
    start = rate * 30
    a = mame[start + coarse: start + coarse + window]
    best_fine, fine = -1.0, 0
    for d in range(-rate // 100, rate // 100 + 1, 4):
        b = port[start + d: start + d + window] if start + d >= 0 else None
        if b is None or len(b) < window or len(a) < window:
            continue
        c = float(np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-12))
        if c > best_fine:
            best_fine, fine = c, d
    for d in range(fine - 3, fine + 4):    # (then to the sample)
        b = port[start + d: start + d + window] if start + d >= 0 else None
        if b is None or len(b) < window or len(a) < window:
            continue
        c = float(np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-12))
        if c > best_fine:
            best_fine, fine = c, d
    offset = coarse - fine    # MAME's sample = the port's + offset
    print(f"lined up: MAME's sound {offset / rate * 1000:+.1f} ms from the port's (envelopes {best:.3f} alike)")
    if offset >= 0:
        mame = mame[offset:]
    else:
        port = port[-offset:]
    seconds = min(len(mame), len(port)) // rate
    if len(sys.argv) > 2:
        seconds = min(seconds, int(float(sys.argv[2])))
    print(f"compared: {seconds} s\n")

    rows = []
    for s in range(seconds):
        a = without_lows(mame[s * rate:(s + 1) * rate], rate)
        b = without_lows(port[s * rate:(s + 1) * rate], rate)
        ra, rb = float(np.sqrt((a ** 2).mean())), float(np.sqrt((b ** 2).mean()))
        if ra < 1e-4 and rb < 1e-4:
            rows.append((s, db(ra), db(rb), 0.0, 1.0, 1.0, (0, 0, 0)))
            continue
        ea, eb = envelope(a, rate, 0.005), envelope(b, rate, 0.005)
        env = float(np.corrcoef(ea, eb)[0, 1]) if ea.std() > 1e-6 and eb.std() > 1e-6 else 1.0
        wave_alike = -1.0
        for d in range(-48, 49):
            x = a[max(0, d): rate + min(0, d)]
            y = b[max(0, -d): rate - max(0, d)]
            m = min(len(x), len(y))
            c = float(np.dot(x[:m], y[:m]) / (np.linalg.norm(x[:m]) * np.linalg.norm(y[:m]) + 1e-12))
            wave_alike = max(wave_alike, c)
        fa, fb = np.abs(np.fft.rfft(a)) ** 2, np.abs(np.fft.rfft(b)) ** 2
        freqs = np.fft.rfftfreq(len(a), 1.0 / rate)
        bands = []
        for lo, hi in ((20, 500), (500, 4000), (4000, 24000)):
            sel = (freqs >= lo) & (freqs < hi)
            bands.append(10 * np.log10((fb[sel].sum() + 1e-12) / (fa[sel].sum() + 1e-12)))
        rows.append((s, db(ra), db(rb), db(rb) - db(ra), env, wave_alike, tuple(bands)))

    loud = [r for r in rows if r[1] > -60 or r[2] > -60]
    if not loud:
        print("no sound in either")
        return
    diffs = np.array([r[3] for r in loud])
    envs = np.array([r[4] for r in loud])
    waves = np.array([r[5] for r in loud])
    bands = np.array([r[6] for r in loud])
    print(f"seconds with sound: {len(loud)}")
    print(f"loudness, the port's against MAME's: median {np.median(diffs):+.2f} dB "
          f"(the middle half {np.percentile(diffs, 25):+.2f} to {np.percentile(diffs, 75):+.2f} dB)")
    print(f"loudness moving alike (envelopes' correlation): median {np.median(envs):.3f}, "
          f"{(envs > 0.9).mean() * 100:.0f}% of seconds over 0.9")
    print(f"the waves alike (correlation at the best offset): median {np.median(waves):.3f}, "
          f"{(waves > 0.9).mean() * 100:.0f}% of seconds over 0.9")
    print("the frequencies, the port's against MAME's (median dB): low (20-500 Hz) {:+.2f}, middle {:+.2f}, "
          "high (over 4 kHz) {:+.2f}".format(*np.median(bands, axis=0)))
    print("\nthe seconds least alike (by the waves):")
    for r in sorted(loud, key=lambda r: r[5])[:12]:
        print(f"  {r[0] // 60}:{r[0] % 60:02d}  MAME {r[1]:6.1f} dB  port {r[2]:6.1f} dB  ({r[3]:+.1f})  "
              f"envelopes {r[4]:.2f}  waves {r[5]:.2f}")
    print("\nminute by minute (median):")
    for minute in range(0, seconds // 60 + 1):
        part = [r for r in loud if r[0] // 60 == minute]
        if part:
            print(f"  {minute:3d}:  loudness {np.median([r[3] for r in part]):+.2f} dB  "
                  f"envelopes {np.median([r[4] for r in part]):.3f}  waves {np.median([r[5] for r in part]):.3f}")

    # the first minute side by side: MAME left, the port right
    n = min(len(mame), len(port), rate * 60)
    both = np.stack([mame[:n], port[:n]], axis=1)
    pcm = np.clip(both * 32767.0, -32768, 32767).astype("<i2")
    with wave.open(folder + "/compare.wav", "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm.tobytes())
    print(f"\n{folder}/compare.wav: the first minute, MAME's on the left, the port's on the right")


if __name__ == "__main__":
    main()
