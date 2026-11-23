#!/usr/bin/env python3
"""Erzeugt aus docs/Gebrauchsanleitung.md zwei PDFs:
  docs/Gebrauchsanleitung.pdf        fuer die Bewohner (ohne Admin-Teile)
  docs/Gebrauchsanleitung-Admin.pdf  fuer den Verwalter vor Ort (alles)

Fassungen im Markdown: Zeilen, die mit <!-- nur admin --> bzw. <!-- nur bewohner -->
enden, gelten nur fuer diese Fassung; Bloecke zwischen <!-- admin --> und
<!-- /admin --> nur fuer den Admin. Sonstige Kommentarzeilen werden ignoriert.

Aufruf (im Projektordner):  python3 tools/make_manual_pdf.py
Benoetigt: reportlab, Pillow, Schrift Liberation Sans (Paket fonts-liberation).
Verwendet Stil und Bausteine von make_handy_pdf.py.

Markdown wie dort, zusaetzlich: mehrere # Kapitel (neue Seite, dunkles Band),
## Abschnitte ohne Nummer, Schritte als Absatz "**1.** Text" und --- (ignoriert).
Der erste Absatz "Gebrauchsanleitung | ... | Stand ..." wird zur Kopfzeile.
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_handy_pdf as base  # noqa: E402  (registriert auch die Schriften)
from make_handy_pdf import (ACCENT, DARK, HEADER_H, INK, LINE, MARGIN, MUTED, PAGE_H,  # noqa: E402
                            PAGE_W, body, colors, inline, intro, md_table, mm, note_box)
from reportlab.lib.pagesizes import A4  # noqa: E402
from reportlab.lib.styles import ParagraphStyle  # noqa: E402
from reportlab.platypus import (CondPageBreak, KeepTogether, ListFlowable, ListItem,  # noqa: E402
                                PageBreak, Paragraph, SimpleDocTemplate, Spacer, Table,
                                TableStyle)

ROOT = base.ROOT
SRC = ROOT / "docs" / "Gebrauchsanleitung.md"
OUTPUTS = {
    "bewohner": ROOT / "docs" / "Gebrauchsanleitung.pdf",
    "admin": ROOT / "docs" / "Gebrauchsanleitung-Admin.pdf",
}
ICON = base.ICON
LOGO = ROOT / "android/app/src/main/res/drawable-nodpi/wg_logo.png"  # optional: eigenes Logo hier ablegen

h1 = ParagraphStyle("h1", fontName="Sans-Bold", fontSize=17, leading=21, textColor=colors.white)
h2 = ParagraphStyle("h2m", fontName="Sans-Bold", fontSize=13, leading=17, textColor=INK)
FULL = PAGE_W - 2 * MARGIN


def chapter(text):
    band = Table([[Paragraph(inline(text), h1)]], colWidths=[FULL])
    band.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), DARK),
        ("LINEBEFORE", (0, 0), (0, -1), 5, ACCENT),
        ("LEFTPADDING", (0, 0), (-1, -1), 12),
        ("TOPPADDING", (0, 0), (-1, -1), 9),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 10),
    ]))
    band.keepWithNext = True
    gap = Spacer(1, 4 * mm)
    gap.keepWithNext = True
    return [band, gap]


def section_heading(text):
    t = Table([[Paragraph(inline(text), h2)]], colWidths=[FULL])
    t.setStyle(TableStyle([
        ("LINEBEFORE", (0, 0), (0, -1), 3, ACCENT),
        ("LINEBELOW", (0, 0), (-1, -1), 0.6, LINE),
        ("LEFTPADDING", (0, 0), (-1, -1), 8),
        ("TOPPADDING", (0, 0), (-1, -1), 1),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
    ]))
    gap = Spacer(1, 3 * mm)
    for f in (t, gap):
        f.keepWithNext = True
    return [Spacer(1, 4 * mm), t, gap]


def steps(items):
    return [ListFlowable(
        [ListItem(Paragraph(inline(t), body), value=int(n), leftIndent=18) for n, t in items],
        bulletType="1", bulletFontName="Sans-Bold", bulletFontSize=10.5, bulletColor=ACCENT,
        leftIndent=18, bulletDedent=18, bulletFormat="%s.", spaceBefore=0, spaceAfter=4)]


def bullets(items):
    return [ListFlowable(
        [ListItem(Paragraph(inline(t), body), leftIndent=16) for t in items],
        bulletType="bullet", bulletFontName="Sans-Bold", bulletFontSize=8, bulletColor=ACCENT,
        leftIndent=16, bulletDedent=16, spaceBefore=0, spaceAfter=4)]


BLOCK_START = re.compile(r"(#|\||>|- |\*\*\d+\.\*\*|---|<!--)")


def parse(md):
    lines = md.splitlines()
    title, subtitle, story, i = "", "", [], 0
    chapters = 0
    first_para = True
    section = []

    def add(block):
        (section if section else story).extend(block)

    def flush():
        nonlocal section
        if section:
            story.append(section[0])
            story.append(KeepTogether(section[1:]))
            section = []

    while i < len(lines):
        line = lines[i].rstrip()
        if not line.strip() or line.strip() == "---":
            i += 1
            continue
        if line.startswith("# "):
            flush()
            chapters += 1
            if chapters == 1:
                title = line[2:].strip()
            else:
                story.append(PageBreak())
                story.extend(chapter(line[2:].strip()))
            i += 1
            continue
        if line.startswith("## "):
            flush()
            section = section_heading(line[3:].strip())
            i += 1
            continue
        if line.startswith("|"):
            rows = []
            while i < len(lines) and lines[i].startswith("|"):
                cells = [c.strip() for c in lines[i].strip().strip("|").split("|")]
                if not all(re.fullmatch(r":?-{3,}:?", c) for c in cells):
                    rows.append(cells)
                i += 1
            add(md_table(rows))
            continue
        if line.startswith(">"):
            text = []
            while i < len(lines) and lines[i].startswith(">"):
                text.append(lines[i].lstrip("> ").strip())
                i += 1
            add(note_box(" ".join(text)))
            continue
        if re.match(r"\*\*\d+\.\*\*", line):
            items = []
            while i < len(lines):
                m = re.match(r"\*\*(\d+)\.\*\*\s*(.*)", lines[i].strip())
                if m:
                    items.append((m.group(1), m.group(2)))
                    i += 1
                elif not lines[i].strip() and i + 1 < len(lines) and \
                        re.match(r"\*\*\d+\.\*\*", lines[i + 1].strip()):
                    i += 1
                else:
                    break
            add(steps(items))
            continue
        if line.startswith("- "):
            items = []
            while i < len(lines) and lines[i].startswith("- "):
                items.append(lines[i][2:].strip())
                i += 1
            add(bullets(items))
            continue
        para = [line.strip()]
        i += 1
        while i < len(lines) and lines[i].strip() and not BLOCK_START.match(lines[i]):
            para.append(lines[i].strip())
            i += 1
        text = " ".join(para)
        if first_para and text.startswith("Gebrauchsanleitung |"):
            subtitle = text
            continue
        add([Paragraph(inline(text), intro if first_para else body)])
        first_para = False
    flush()
    return title, subtitle, story


def variant_text(md, variant):
    out, in_admin = [], False
    for line in md.splitlines():
        stripped = line.strip()
        if stripped == "<!-- admin -->":
            in_admin = True
            continue
        if stripped == "<!-- /admin -->":
            in_admin = False
            continue
        if in_admin and variant != "admin":
            continue
        m = re.search(r"\s*<!--\s*nur (admin|bewohner)\s*-->\s*$", line)
        if m:
            if m.group(1) != variant:
                continue
            line = line[:m.start()]
        elif stripped.startswith("<!--") and stripped.endswith("-->") and "bild:" not in stripped:
            continue
        out.append(line)
    return "\n".join(out)


def main():
    md = SRC.read_text(encoding="utf-8")
    for variant, out in OUTPUTS.items():
        build(variant_text(md, variant), out, variant == "admin")


def build(md, OUT, admin):
    title, subtitle, story = parse(md)
    parts = [p.strip() for p in subtitle.split("|")] if subtitle else ["Gebrauchsanleitung"]
    stand = next((p for p in parts if p.startswith("Stand")), "")

    def footer(c, doc):
        c.saveState()
        c.setStrokeColor(LINE)
        c.setLineWidth(0.6)
        c.line(MARGIN, 13 * mm, PAGE_W - MARGIN, 13 * mm)
        c.setFont("Sans", 8.5)
        c.setFillColor(MUTED)
        c.drawString(MARGIN, 8.5 * mm, f"Open Intercom · Gebrauchsanleitung{' (Admin)' if admin else ''} · {stand}")
        c.drawRightString(PAGE_W - MARGIN, 8.5 * mm, f"Seite {doc.page}")
        c.restoreState()

    def first_page(c, doc):
        c.saveState()
        c.setFillColor(DARK)
        c.rect(0, PAGE_H - HEADER_H, PAGE_W, HEADER_H, stroke=0, fill=1)
        icon_h = 34 * mm
        c.drawImage(str(ICON), MARGIN, PAGE_H - HEADER_H + (HEADER_H - icon_h) / 2,
                    width=icon_h * 316 / 366, height=icon_h)
        x = MARGIN + icon_h * 316 / 366 + 9 * mm
        c.setFillColor(colors.HexColor("#9fb4d8"))
        c.setFont("Sans-Bold", 10)
        c.drawString(x, PAGE_H - 19 * mm, "OPEN INTERCOM · " + parts[0].upper() + (" · ADMIN" if admin else ""))
        c.setFillColor(colors.white)
        c.setFont("Sans-Bold", 22)
        c.drawString(x, PAGE_H - 30 * mm, title)
        c.setFont("Sans", 13)
        c.drawString(x, PAGE_H - 39 * mm, " · ".join(parts[1:]))
        # Logo rechts im Kopf, darunter "© by Willi" (wie oben links in der Tablet-App)
        logo_h = 27 * mm
        logo_w = logo_h * 785 / 755
        lx = PAGE_W - MARGIN - logo_w
        if LOGO.exists():  # eigenes Logo optional, siehe LOGO oben
            c.drawImage(str(LOGO), lx, PAGE_H - 8 * mm - logo_h, width=logo_w, height=logo_h,
                        mask="auto")
        c.setFont("Sans", 9)
        c.drawCentredString(lx + logo_w / 2, PAGE_H - 8 * mm - logo_h - 5.5 * mm, "© by Willi")
        c.restoreState()
        footer(c, doc)

    doc = SimpleDocTemplate(str(OUT), pagesize=A4, leftMargin=MARGIN, rightMargin=MARGIN,
                            topMargin=MARGIN, bottomMargin=20 * mm, title=f"{title} – Gebrauchsanleitung",
                            author="Open Intercom", subject="Gebrauchsanleitung")
    story.insert(0, Spacer(1, HEADER_H - MARGIN + 4 * mm))
    doc.build(story, onFirstPage=first_page, onLaterPages=footer)
    print(f"geschrieben: {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    sys.exit(main())
