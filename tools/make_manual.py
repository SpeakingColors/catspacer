#!/usr/bin/env python3
"""Builds the short PDF manual of a cat plugin from its own source (controls, ranges, info-bubble texts, presets)
plus docs/manual.txt (intro and tips). Usage: python3 tools/make_manual.py <plugin .cpp> <docs/manual.txt> <out.pdf>
Needs: pip install reportlab"""
import re, sys
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, KeepTogether
from reportlab.graphics.shapes import Drawing, Polygon, Circle, Rect

src_path, txt_path, out = sys.argv[1:4]
src = open(src_path, encoding="utf-8").read()
src = re.sub(r'"\s*\n\s*"', '', src)   # join split string literals

name = re.search(r'FxBase\("([^"]+)"', src).group(1)
S = r'"((?:[^"\\]|\\.)*)"'
num = r'(-?[\d.]+)f?'
knobs = {m.group(1): m.groups() for m in re.finditer(r'\{\s*"(\w+)",\s*"([^"]+)",\s*"([^"]*)",\s*' + num + r',\s*' + num + r',\s*' + num + r',\s*' + num + r',\s*' + S + r'\s*\}', src)}
choices = {m.group(1): m.groups() for m in re.finditer(r'\{\s*"(\w+)",\s*"([^"]+)",\s*\{([^}]*)\},\s*(\d+),\s*' + S + r'\s*\}', src)}
panels = []
pm = re.search(r'::panels\(\) const\s*\{(.*?)\n\}', src, re.S)
if pm:
    for m in re.finditer(r'\{\s*"([^"]+)",\s*\{([^}]*)\},\s*\{([^}]*)\},\s*[\d.]+f?', pm.group(1)):
        ids = lambda t: re.findall(r'"(\w+)"', t)
        panels.append((m.group(1), ids(m.group(2)), ids(m.group(3))))
if not panels:
    panels = [("Controls", list(knobs), list(choices))]
presets = []
pr = re.search(r'::presets\(\) const\s*\{(.*?)\n\}', src, re.S)
if pr: presets = re.findall(r'\{\s*"([^"]+)",\s*\{', pr.group(1))

sec = {}
cur = None
for line in open(txt_path, encoding="utf-8").read().splitlines():
    if line.startswith("# "): cur = line[2:].strip(); sec[cur] = []
    elif cur is not None: sec[cur].append(line)
para = lambda k: [p.strip() for p in "\n".join(sec.get(k, [])).split("\n\n") if p.strip()]
bullets = lambda k: [l[2:].strip() for l in sec.get(k, []) if l.startswith("- ")]

NAVY, TEAL, PINK, INK = colors.HexColor("#14204a"), colors.HexColor("#01c9ae"), colors.HexColor("#e0147a"), colors.HexColor("#1b1b1f")
st = lambda n, **k: ParagraphStyle(n, fontName=k.pop("font", "Helvetica"), **k)
H1 = st("h1", fontSize=24, leading=28, textColor=NAVY, font="Helvetica-Bold")
H2 = st("h2", fontSize=14, leading=18, textColor=PINK, font="Helvetica-Bold", spaceBefore=10, spaceAfter=4)
B = st("b", fontSize=10, leading=14, textColor=INK)
SM = st("s", fontSize=8.5, leading=11.5, textColor=colors.HexColor("#444444"))
CELL = st("c", fontSize=9, leading=12, textColor=INK)
CELLB = st("cb", fontSize=9, leading=12, textColor=NAVY, font="Helvetica-Bold")

def logo():
    d = Drawing(40, 36)
    d.add(Polygon([4, 18, 7, 34, 18, 24], fillColor=TEAL, strokeColor=None)); d.add(Polygon([36, 18, 33, 34, 22, 24], fillColor=TEAL, strokeColor=None))
    d.add(Circle(20, 14, 14, fillColor=TEAL, strokeColor=None))
    d.add(Circle(14, 16, 2, fillColor=NAVY, strokeColor=None)); d.add(Circle(26, 16, 2, fillColor=NAVY, strokeColor=None))
    return d

esc = lambda t: t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
def fmt(v):
    f = float(v); return str(int(f)) if f == int(f) else ("%g" % f)

story = []
head = Table([[logo(), [Paragraph(esc(name), H1), Paragraph("Short manual . free and open source (AGPL-3.0-or-later) . github.com/SpeakingColors", SM)]]], colWidths=[18 * mm, 150 * mm])
head.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "MIDDLE"), ("LEFTPADDING", (0, 0), (-1, -1), 0)]))
story += [head, Spacer(1, 6)]
for p in para("Intro"): story += [Paragraph(esc(p), B), Spacer(1, 4)]
story.append(Paragraph("Quick start", H2))
for t in bullets("Quick start") or ["Insert the plugin on a track or bus, pick a preset, then adjust the knobs while you listen."]:
    story.append(Paragraph("&bull; " + esc(t), B))
story.append(Paragraph("How the window works", H2))
for t in ["Each knob sits directly under the display it changes; the displays react to the knobs and the knobs react to the displays.",
          "Meters show the real signal in dB with a dB scale: input and output on the far right.",
          "Hover a control for about one second to see an info bubble. Turn bubbles off with the round i button in the header.",
          "The Limiter switch in the header adds a safety limiter at the output (useful while experimenting).",
          "Presets are in the header; the dice picks random settings (some controls are excluded on purpose)."]:
    story.append(Paragraph("&bull; " + esc(t), B))
story.append(Paragraph("Controls", H2))
for title, kids, chs in panels:
    rows = []
    for c in chs:
        if c in choices:
            g = choices[c]; items = [i.strip().strip('"') for i in g[2].split(",")]
            opts = ", ".join(items) if len(items) > 2 else " / ".join(items)
            rows.append([Paragraph(esc(g[1]), CELLB), Paragraph("Choice: " + esc(opts) + " (default " + esc(items[int(g[3])]) + ")", CELL), Paragraph(esc(g[4]), CELL)])
    for k in kids:
        if k in knobs:
            g = knobs[k]
            rows.append([Paragraph(esc(g[1]), CELLB), Paragraph("%s to %s%s<br/>default %s%s" % (fmt(g[3]), fmt(g[4]), esc(g[2]), fmt(g[5]), esc(g[2])), CELL), Paragraph(esc(g[7]), CELL)])
    if not rows: continue
    t = Table(rows, colWidths=[28 * mm, 40 * mm, 102 * mm], repeatRows=0)
    t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LINEBELOW", (0, 0), (-1, -1), 0.4, colors.HexColor("#d6d8ea")),
                           ("TOPPADDING", (0, 0), (-1, -1), 3), ("BOTTOMPADDING", (0, 0), (-1, -1), 3), ("LEFTPADDING", (0, 0), (-1, -1), 2)]))
    story.append(KeepTogether([Paragraph(esc(title), st("ph", fontSize=11.5, leading=15, textColor=TEAL, font="Helvetica-Bold", spaceBefore=6, spaceAfter=2)), t]))
tips = bullets("Recommendations")
if tips:
    story.append(Paragraph("Recommendations", H2))
    for t in tips: story.append(Paragraph("&bull; " + esc(t), B))
if presets:
    story.append(Paragraph("Presets", H2))
    story.append(Paragraph(esc(", ".join(presets)), B))
story.append(Paragraph("Install and use", H2))
for t in ["Windows: run the installer (.exe). macOS: open the installer (.pkg or .dmg). Linux: install the Flatpak with <font name='Courier'>flatpak install --user</font> and the .flatpak file, or copy the .vst3 folder to ~/.vst3.",
          "Everything works offline. Rescan plugins in your DAW, then look for the plugin under SpeakingColors.",
          "Report problems or ideas on the GitHub page of the plugin. Free to use, copy and change under the AGPL-3.0-or-later licence."]:
    story.append(Paragraph("&bull; " + t, B))

def footer(c, d):
    c.saveState(); c.setFont("Helvetica", 8); c.setFillColor(colors.HexColor("#777777"))
    c.drawString(20 * mm, 10 * mm, name + " manual"); c.drawRightString(190 * mm, 10 * mm, "page %d" % d.page); c.restoreState()
SimpleDocTemplate(out, pagesize=A4, leftMargin=20 * mm, rightMargin=20 * mm, topMargin=16 * mm, bottomMargin=18 * mm, title=name + " manual", author="SpeakingColors").build(story, onFirstPage=footer, onLaterPages=footer)
print("wrote", out, "knobs", len(knobs), "choices", len(choices), "panels", len(panels), "presets", len(presets))
