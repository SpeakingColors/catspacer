#!/usr/bin/env python3
"""Builds the short PDF manual of a cat plugin from its own source (controls, ranges, info-bubble texts, presets)
plus docs/manual.txt (intro and tips). Usage: python3 tools/make_manual.py <plugin .cpp> <docs/manual.txt> <out.pdf>
Needs: pip install reportlab"""
import re, sys, os
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

# Plugin look: window navy 0xff161638, panels 0xff2C2C62 (popNavy), titles mistLilac, accent amber (cat-ui)
BG, PANEL = colors.HexColor("#161638"), colors.HexColor("#2c2c62")
NAVY, TEAL, PINK, INK = colors.HexColor("#ffa600"), colors.HexColor("#e1cbf4"), colors.HexColor("#ff7bb1"), colors.HexColor("#dcdaf5")
PURPLE, LILAC, DIM = colors.HexColor("#ffa600"), colors.HexColor("#e1cbf4"), colors.HexColor("#9b98c8")
st = lambda n, **k: ParagraphStyle(n, fontName=k.pop("font", "Helvetica"), **k)
H1 = st("h1", fontSize=24, leading=28, textColor=NAVY, font="Helvetica-Bold")
H2 = st("h2", fontSize=14, leading=18, textColor=LILAC, font="Helvetica-Bold", spaceBefore=10, spaceAfter=4)
B = st("b", fontSize=10, leading=14, textColor=INK)
SM = st("s", fontSize=8.5, leading=11.5, textColor=DIM)
CELL = st("c", fontSize=9, leading=12, textColor=INK)
CELLB = st("cb", fontSize=9, leading=12, textColor=PINK, font="Helvetica-Bold")

def logo():
    """Draws assets/icon.svg (the plugin's own cat head) with reportlab shapes."""
    import os, xml.etree.ElementTree as ET
    from reportlab.graphics.shapes import Path, Group
    svg = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "icon.svg")
    root = ET.parse(svg).getroot(); K = 15.0 / 128.0 * mm * 1.0
    d = Drawing(15 * mm, 15 * mm); g = Group(); g.transform = (K, 0, 0, -K, 0, 15 * mm); d.add(g)
    col = lambda v: None if v in (None, "none") else colors.HexColor(v)
    def style(e, o):
        f, sk = e.get("fill"), e.get("stroke")
        o.fillColor = col(f) if f else (None if sk else colors.black); o.strokeColor = col(sk)
        o.strokeWidth = float(e.get("stroke-width", 1)); o.strokeLineCap = 1 if e.get("stroke-linecap") == "round" else 0; o.strokeLineJoin = 1 if e.get("stroke-linejoin") == "round" else 0
        if e.get("stroke-dasharray"): o.strokeDashArray = [float(x) for x in e.get("stroke-dasharray").split()]
        return o
    for e in root.iter():
        t = e.tag.split("}")[1]
        if t == "rect":
            r = Rect(float(e.get("x", 0)), float(e.get("y", 0)), float(e.get("width")), float(e.get("height")), rx=float(e.get("rx", 0)), ry=float(e.get("rx", 0))); g.add(style(e, r))
        elif t == "circle": g.add(style(e, Circle(float(e.get("cx")), float(e.get("cy")), float(e.get("r")))))
        elif t == "ellipse":
            from reportlab.graphics.shapes import Ellipse
            g.add(style(e, Ellipse(float(e.get("cx")), float(e.get("cy")), float(e.get("rx")), float(e.get("ry")))))
        elif t == "path":
            toks = re.findall(r"[MLQTZmlqtz]|-?[\d.]+", e.get("d")); i = 0; cmd = None; cur = (0, 0); start = (0, 0); lastc = None
            P = Path(); P.points = []; P.operators = []
            def num():
                nonlocal i; v = float(toks[i]); i += 1; return v
            while i < len(toks):
                if toks[i].isalpha(): cmd = toks[i]; i += 1
                if cmd == "M":
                    cur = (num(), num()); start = cur; P.moveTo(*cur); cmd = "L"; lastc = None
                elif cmd == "L":
                    cur = (num(), num()); P.lineTo(*cur); lastc = None
                elif cmd in "QT":
                    if cmd == "Q": c = (num(), num())
                    else: c = (2 * cur[0] - lastc[0], 2 * cur[1] - lastc[1]) if lastc else cur
                    en = (num(), num())
                    P.curveTo(cur[0] + 2 / 3 * (c[0] - cur[0]), cur[1] + 2 / 3 * (c[1] - cur[1]), en[0] + 2 / 3 * (c[0] - en[0]), en[1] + 2 / 3 * (c[1] - en[1]), *en); cur = en; lastc = c
                elif cmd in "Zz": P.closePath(); cur = start; lastc = None
                else: i += 1
            g.add(style(e, P))
    return d

esc = lambda t: t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
def fmt(v):
    f = float(v); return str(int(f)) if f == int(f) else ("%g" % f)

story = []
head = Table([[logo(), [Paragraph(esc(name), H1), Paragraph("Short manual . free and open source (AGPL-3.0-or-later) . github.com/SpeakingColors", SM)]]], colWidths=[19 * mm, 150 * mm])
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
    t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("BACKGROUND", (0, 0), (-1, -1), PANEL), ("LINEBELOW", (0, 0), (-1, -1), 0.5, colors.HexColor("#161638")),
                           ("TOPPADDING", (0, 0), (-1, -1), 5), ("BOTTOMPADDING", (0, 0), (-1, -1), 5), ("LEFTPADDING", (0, 0), (-1, -1), 6)]))
    story.append(KeepTogether([Paragraph(esc(title), st("ph", fontSize=11.5, leading=15, textColor=TEAL, font="Helvetica-Bold", spaceBefore=8, spaceAfter=4)), t]))
tips = bullets("Recommendations")
if tips:
    story.append(Paragraph("Recommendations", H2))
    for t in tips: story.append(Paragraph("&bull; " + esc(t), B))
if presets:
    story.append(Paragraph("Presets", H2))
    story.append(Paragraph(esc(", ".join(presets)), B))
_tdir = os.path.join(os.path.dirname(os.path.abspath(txt_path)), "tour")
if os.path.exists(os.path.join(_tdir, "key.txt")):
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from tour_pages import tour_story
    story += tour_story(_tdir, open(os.path.join(_tdir, "key.txt")).read().strip(), H2, B, CELL, CELLB, SM, TEAL, PANEL, esc, st)
story.append(Paragraph("Install and use", H2))
for t in ["Windows: run the installer (.exe). macOS: open the installer (.pkg or .dmg). Linux: install the Flatpak with <font name='Courier'>flatpak install --user</font> and the .flatpak file, or copy the .vst3 folder to ~/.vst3.",
          "Everything works offline. Rescan plugins in your DAW, then look for the plugin under Studio 87.",
          "Report problems or ideas on the GitHub page of the plugin. Free to use, copy and change under the AGPL-3.0-or-later licence."]:
    story.append(Paragraph("&bull; " + t, B))

def footer(c, d):
    c.saveState(); c.setFillColor(BG); c.rect(0, 0, A4[0], A4[1], stroke=0, fill=1)
    c.setFillColor(PANEL); c.rect(0, A4[1] - 5 * mm, A4[0], 5 * mm, stroke=0, fill=1)
    c.setFillColor(PURPLE); c.rect(0, A4[1] - 5.8 * mm, A4[0], 0.8 * mm, stroke=0, fill=1)
    c.setFont("Helvetica", 8); c.setFillColor(DIM)
    c.drawString(20 * mm, 10 * mm, name + " manual"); c.drawRightString(190 * mm, 10 * mm, "page %d" % d.page); c.restoreState()
SimpleDocTemplate(out, pagesize=A4, leftMargin=20 * mm, rightMargin=20 * mm, topMargin=16 * mm, bottomMargin=18 * mm, title=name + " manual", author="Studio 87").build(story, onFirstPage=footer, onLaterPages=footer)
print("wrote", out, "knobs", len(knobs), "choices", len(choices), "panels", len(panels), "presets", len(presets))
