#!/usr/bin/env python3
"""Erzeugt firmware/doorstation_v2/DefaultAnnouncements.h aus den Ansage-Dateien.

Die Saetze werden beim ersten Start der Tuerstation in die Satzsammlung
geschrieben (nur wenn die Sammlung noch nie befuellt wurde).
Umwandlung: ffmpeg -> 16 Bit Mono PCM mit RATE Hz, Spitze auf 90 % normalisiert,
wie es auch die Admin-Seite /ansage im Browser macht.

Aufruf (im Projektordner):  python3 tools/make_default_announcements.py
"""
import array
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "firmware" / "doorstation_v2" / "DefaultAnnouncements.h"
RATE = 44100  # CD-Qualitaet
SENTENCES = [
    ("Nicht zu Hause – andere Etage", ROOT / "Sprache" / "nicht_da_andere_etage.mp3"),
    ("Nicht da – am Handy erreichbar", ROOT / "Sprache" / "nicht_zuhause_auf_dem_handy_erreichbar.mp3"),
]


def pcm(path):
    raw = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", str(path), "-ac", "1", "-ar", str(RATE),
         "-af", "aresample=resampler=soxr", "-f", "s16le", "-"],
        check=True, capture_output=True).stdout
    samples = array.array("h", raw)
    if sys.byteorder != "little":
        samples.byteswap()
    peak = max(1, max(abs(s) for s in samples))
    gain = 0.9 * 32767 / peak
    out = array.array("h", (max(-32768, min(32767, round(s * gain))) for s in samples))
    if sys.byteorder != "little":
        out.byteswap()
    return out.tobytes()


def main():
    parts = ["#pragma once",
             "// Erzeugt von tools/make_default_announcements.py – nicht von Hand aendern.",
             f"// {len(SENTENCES)} Saetze, 16 Bit Mono PCM, {RATE} Hz.",
             "#include <stdint.h>", "#include <stddef.h>", ""]
    table = []
    for i, (name, path) in enumerate(SENTENCES, 1):
        data = pcm(path)
        print(f"{name}: {len(data) / (RATE * 2):.2f} s, {len(data)} Bytes")
        lines = []
        for off in range(0, len(data), 24):
            lines.append("  " + ",".join(str(b) for b in data[off:off + 24]) + ",")
        parts.append(f"static const uint8_t ANN_DEFAULT_{i}[{len(data)}] = {{")
        parts.extend(lines)
        parts.append("};")
        table.append(f'  {{"{name}", {RATE}, ANN_DEFAULT_{i}, sizeof(ANN_DEFAULT_{i})}},')
    parts += ["", "struct AnnDefault { const char* name; uint32_t rate; const uint8_t* data; size_t bytes; };",
              f"static const AnnDefault ANN_DEFAULTS[{len(SENTENCES)}] = {{", *table, "};", ""]
    OUT.write_text("\n".join(parts), encoding="utf-8")
    print(f"geschrieben: {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
