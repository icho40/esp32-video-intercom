#!/usr/bin/env python3
"""Erzeugt einfache, frei verwendbare Klingeltöne für die Apps.

Die Töne werden rein synthetisch berechnet (gedämpfte Sinusklänge) und
stehen damit unter derselben Lizenz wie dieses Projekt (GPL-3.0-or-later).
Ausgabe: res/raw/klingel_p1.mp3 … klingel_p3.mp3 in beiden Android-Apps.
Benötigt numpy und das Kommandozeilenprogramm lame.

Eigene Töne: einfach die MP3-Dateien unter gleichem Namen ersetzen.
"""
import subprocess, tempfile, wave
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parent.parent
RATE = 48000

# Je Klingel eine Tonfolge: (Frequenz in Hz, Einsatz in s)
MELODIEN = {
    1: [(659.25, 0.0), (523.25, 0.55)],                   # E5 – C5  (Ding-Dong)
    2: [(523.25, 0.0), (659.25, 0.35), (783.99, 0.70)],   # C5 – E5 – G5
    3: [(440.00, 0.0), (349.23, 0.55)],                   # A4 – F4  (tiefer)
}
DAUER = 3.0


def glocke(freq: float, t: np.ndarray) -> np.ndarray:
    """Gedämpfter Klang mit zwei Obertönen."""
    huelle = np.exp(-t * 2.2) * np.minimum(1.0, t / 0.005)
    return huelle * (np.sin(2 * np.pi * freq * t)
                     + 0.35 * np.sin(2 * np.pi * 2.0 * freq * t)
                     + 0.12 * np.sin(2 * np.pi * 3.0 * freq * t))


def erzeuge(nummer: int) -> np.ndarray:
    n = int(DAUER * RATE)
    signal = np.zeros(n)
    for freq, start in MELODIEN[nummer]:
        s = int(start * RATE)
        t = np.arange(n - s) / RATE
        signal[s:] += glocke(freq, t)
    signal /= np.max(np.abs(signal))
    return (signal * 0.8 * 32767).astype(np.int16)


def main() -> None:
    for nummer in MELODIEN:
        pcm = erzeuge(nummer)
        with tempfile.NamedTemporaryFile(suffix=".wav") as tmp:
            with wave.open(tmp.name, "wb") as w:
                w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE)
                w.writeframes(pcm.tobytes())
            for app in ("android", "android-phone"):
                ziel = ROOT / app / "app/src/main/res/raw" / f"klingel_p{nummer}.mp3"
                ziel.parent.mkdir(parents=True, exist_ok=True)
                subprocess.run(["lame", "--quiet", "-b", "128", tmp.name, str(ziel)], check=True)
                print(ziel.relative_to(ROOT))


if __name__ == "__main__":
    main()
