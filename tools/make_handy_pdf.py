#!/usr/bin/env python3
"""Erzeugt docs/Handy-App-Installation.pdf aus docs/Handy-App-Installation.md.

Aufruf (im Projektordner):  python3 tools/make_handy_pdf.py
Benoetigt: reportlab, Pillow, Schrift Liberation Sans (Paket fonts-liberation).

Unterstuetztes Markdown (bewusst klein gehalten): # Titel, ## Abschnitt,
Absaetze, 1. nummerierte Listen, - Aufzaehlungen, Tabellen, > Hinweise,
**fett**, `Code` und Bilder als Kommentar:
<!-- bild: images/datei.png | Bildunterschrift -->
"""
import re
import sys
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (Flowable, Image, KeepTogether, ListFlowable, ListItem,
                                Paragraph, SimpleDocTemplate, Spacer, Table, TableStyle)

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "docs" / "Handy-App-Installation.md"
OUT = ROOT / "docs" / "Handy-App-Installation.pdf"
ICON = ROOT / "android-phone/app/src/main/res/drawable-nodpi/intercom_idle.png"
STAND = "Stand 10.11.2026"

FONT_DIR = Path("/usr/share/fonts/truetype/liberation")
pdfmetrics.registerFont(TTFont("Sans", str(FONT_DIR / "LiberationSans-Regular.ttf")))
pdfmetrics.registerFont(TTFont("Sans-Bold", str(FONT_DIR / "LiberationSans-Bold.ttf")))
pdfmetrics.registerFont(TTFont("Sans-Italic", str(FONT_DIR / "LiberationSans-Italic.ttf")))
pdfmetrics.registerFont(TTFont("Mono", str(FONT_DIR / "LiberationMono-Regular.ttf")))
pdfmetrics.registerFontFamily("Sans", normal="Sans", bold="Sans-Bold", italic="Sans-Italic",
                              boldItalic="Sans-Bold")

INK = colors.HexColor("#1d2330")
MUTED = colors.HexColor("#5b6475")
ACCENT = colors.HexColor("#1f6feb")
ACCENT_SOFT = colors.HexColor("#e8f0fe")
WARN = colors.HexColor("#b26a00")
WARN_SOFT = colors.HexColor("#fff4e0")
LINE = colors.HexColor("#d9dee7")
ZEBRA = colors.HexColor("#f5f7fa")
DARK = colors.HexColor("#111318")

PAGE_W, PAGE_H = A4
MARGIN = 18 * mm
HEADER_H = 52 * mm

body = ParagraphStyle("body", fontName="Sans", fontSize=10.5, leading=14.5, textColor=INK,
                      spaceAfter=4)
small = ParagraphStyle("small", parent=body, fontSize=9.5, leading=13)
cell = ParagraphStyle("cell", parent=body, fontSize=9.5, leading=12.5, spaceAfter=0)
cell_head = ParagraphStyle("cellhead", parent=cell, fontName="Sans-Bold", textColor=colors.white)
h2 = ParagraphStyle("h2", fontName="Sans-Bold", fontSize=14, leading=18, textColor=INK)
caption = ParagraphStyle("caption", parent=small, textColor=MUTED, alignment=TA_CENTER,
                         fontName="Sans-Italic")
intro = ParagraphStyle("intro", parent=body, fontSize=11.5, leading=16.5)


def inline(text):
    """Markdown-Inline (**fett**, `code`) nach ReportLab-Markup."""
    text = text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    text = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", text)
    text = re.sub(r"`(.+?)`", r'<font face="Mono" size="9">\1</font>', text)
    return text


class StepBadge(Flowable):
    """Runde Nummer vor einer Abschnittsueberschrift."""

    def __init__(self, label, color, size=9 * mm):
        super().__init__()
        self.label, self.color, self.size = label, color, size
        self.width = self.height = size

    def draw(self):
        c = self.canv
        c.setFillColor(self.color)
        c.circle(self.size / 2, self.size / 2, self.size / 2, stroke=0, fill=1)
        c.setFillColor(colors.white)
        c.setFont("Sans-Bold", 13 if len(self.label) == 1 else 9)
        c.drawCentredString(self.size / 2, self.size / 2 - 4.5, self.label)


def heading(text):
    m = re.match(r"(\d+)\.\s+(.*)", text)
    samsung = "Samsung" in text
    color = WARN if samsung else ACCENT
    if m:
        badge = StepBadge(m.group(1), color)
        title = m.group(2)
    else:
        badge = StepBadge("?", MUTED) if "klappt" in text else StepBadge("i", MUTED)
        title = text
    t = Table([[badge, Paragraph(inline(title), h2)]], colWidths=[12 * mm, None])
    t.setStyle(TableStyle([
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
        ("LEFTPADDING", (0, 0), (-1, -1), 0),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 2),
        ("TOPPADDING", (0, 0), (-1, -1), 0),
    ]))
    rule = Table([[""]], colWidths=[PAGE_W - 2 * MARGIN], rowHeights=[0.6],
                 style=[("LINEBELOW", (0, 0), (-1, -1), 0.6, LINE)])
    gap = Spacer(1, 3 * mm)
    # Ueberschrift, Linie und Abstand bleiben mit dem folgenden Inhalt zusammen.
    for f in (t, rule, gap):
        f.keepWithNext = True
    return [Spacer(1, 4 * mm), t, rule, gap], samsung


def note_box(text, warn=False):
    bg, bar = (WARN_SOFT, WARN) if warn else (ACCENT_SOFT, ACCENT)
    t = Table([[Paragraph(inline(text), small)]], colWidths=[PAGE_W - 2 * MARGIN])
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), bg),
        ("LINEBEFORE", (0, 0), (0, -1), 3, bar),
        ("LEFTPADDING", (0, 0), (-1, -1), 9),
        ("RIGHTPADDING", (0, 0), (-1, -1), 9),
        ("TOPPADDING", (0, 0), (-1, -1), 7),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 7),
    ]))
    return [Spacer(1, 2 * mm), t, Spacer(1, 3 * mm)]


def md_table(rows):
    data = [[Paragraph(inline(c), cell_head) for c in rows[0]]]
    data += [[Paragraph(inline(c), cell) for c in r] for r in rows[1:]]
    ncol = len(rows[0])
    avail = PAGE_W - 2 * MARGIN
    widths = {2: [0.36, 0.64], 3: [0.27, 0.35, 0.38]}.get(ncol, [1 / ncol] * ncol)
    t = Table(data, colWidths=[w * avail for w in widths], repeatRows=1)
    style = [
        ("BACKGROUND", (0, 0), (-1, 0), INK),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 7),
        ("RIGHTPADDING", (0, 0), (-1, -1), 7),
        ("TOPPADDING", (0, 0), (-1, -1), 4),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
        ("LINEBELOW", (0, 1), (-1, -1), 0.4, LINE),
    ]
    for r in range(2, len(data), 2):
        style.append(("BACKGROUND", (0, r), (-1, r), ZEBRA))
    t.setStyle(TableStyle(style))
    return [Spacer(1, 1 * mm), t, Spacer(1, 3 * mm)]


def picture(path, text):
    img_path = (SRC.parent / path).resolve()
    from PIL import Image as PILImage
    w, h = PILImage.open(img_path).size
    target_h = 70 * mm
    img = Image(str(img_path), width=target_h * w / h, height=target_h)
    frame = Table([[img]], style=[
        ("BOX", (0, 0), (-1, -1), 0.8, LINE),
        ("LEFTPADDING", (0, 0), (-1, -1), 0), ("RIGHTPADDING", (0, 0), (-1, -1), 0),
        ("TOPPADDING", (0, 0), (-1, -1), 0), ("BOTTOMPADDING", (0, 0), (-1, -1), 0),
    ])
    return [Spacer(1, 2 * mm), frame, Spacer(1, 2 * mm),
            Paragraph(inline(text), caption), Spacer(1, 3 * mm)]


def parse(md):
    lines = md.splitlines()
    title, story, i = "", [], 0
    in_samsung = False
    first_para = True
    section = []  # aktueller Abschnitt: Ueberschrift + Inhalt, wird zusammengehalten

    def add(block):
        (section if section else story).extend(block)

    def flush():
        nonlocal section
        if section:
            story.append(section[0])               # Abstand vor der Ueberschrift darf umbrechen
            story.append(KeepTogether(section[1:]))
            section = []
    while i < len(lines):
        line = lines[i].rstrip()
        if not line.strip():
            i += 1
            continue
        if line.startswith("# "):
            title = line[2:].strip()
            i += 1
            continue
        if line.startswith("## "):
            flush()
            section, in_samsung = heading(line[3:].strip())
            i += 1
            continue
        m = re.match(r"<!--\s*bild:\s*(.+?)\s*\|\s*(.+?)\s*-->", line)
        if m:
            add(picture(m.group(1), m.group(2)))
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
            add(note_box(" ".join(text), warn=in_samsung))
            continue
        if re.match(r"\d+\.\s", line) or line.startswith("- "):
            numbered = bool(re.match(r"\d+\.\s", line))
            items = []
            while i < len(lines) and (re.match(r"\d+\.\s", lines[i]) or lines[i].startswith("- ")):
                text = re.sub(r"^(\d+\.|-)\s+", "", lines[i].strip())
                items.append(ListItem(Paragraph(inline(text), body), leftIndent=16))
                i += 1
            color = WARN if in_samsung else ACCENT
            add([ListFlowable(
                items, bulletType="1" if numbered else "bullet",
                start="1" if numbered else None,
                bulletFontName="Sans-Bold", bulletFontSize=10.5 if numbered else 8,
                bulletColor=color, leftIndent=16, bulletDedent=16,
                bulletFormat="%s." if numbered else None,
                spaceBefore=0, spaceAfter=4)])
            continue
        para = [line.strip()]
        i += 1
        while i < len(lines) and lines[i].strip() and not re.match(r"(#|\||>|- |\d+\.\s|<!--)", lines[i]):
            para.append(lines[i].strip())
            i += 1
        style = intro if first_para else body
        first_para = False
        add([Paragraph(inline(" ".join(para)), style)])
    flush()
    return title, story


def main():
    md = SRC.read_text(encoding="utf-8")
    title, story = parse(md)

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
        c.drawString(x, PAGE_H - 19 * mm, "OPEN INTERCOM · INSTALLATIONSANLEITUNG")
        c.setFillColor(colors.white)
        c.setFont("Sans-Bold", 22)
        parts = title.split(" – ")
        c.drawString(x, PAGE_H - 30 * mm, parts[0])
        if len(parts) > 1:
            c.setFont("Sans", 15)
            c.drawString(x, PAGE_H - 39 * mm, parts[1])
        c.restoreState()
        footer(c, doc)

    def footer(c, doc):
        c.saveState()
        c.setStrokeColor(LINE)
        c.setLineWidth(0.6)
        c.line(MARGIN, 13 * mm, PAGE_W - MARGIN, 13 * mm)
        c.setFont("Sans", 8.5)
        c.setFillColor(MUTED)
        c.drawString(MARGIN, 8.5 * mm, f"Open Intercom · Handy-Apps installieren · {STAND}")
        c.drawRightString(PAGE_W - MARGIN, 8.5 * mm, f"Seite {doc.page}")
        c.restoreState()

    doc = SimpleDocTemplate(str(OUT), pagesize=A4, leftMargin=MARGIN, rightMargin=MARGIN,
                            topMargin=MARGIN, bottomMargin=20 * mm,
                            title=title, author="Open Intercom",
                            subject="Installationsanleitung für die Handy-Apps")
    story.insert(0, Spacer(1, HEADER_H - MARGIN + 4 * mm))
    doc.build(story, onFirstPage=first_page, onLaterPages=footer)
    print(f"geschrieben: {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    sys.exit(main())
