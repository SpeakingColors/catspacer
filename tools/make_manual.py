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
knobs = {m.group(1): m.groups() for m in re.finditer(r'\{\s*"(\w+)",\s*"([^"]+)",\s*"([^"]*)",\s*' + num + r',\s*' + num + r',\s*' + num + r',\s*' + num + r',\s*' + S + r'(?:,\s*' + num + r')?\s*\}', src)}
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
    """Draws assets/icon.svg (the plugin's own icon) with reportlab shapes: rect, circle, ellipse, path (M L H V Q T C S Z, absolute and relative), groups, fill inheritance, opacity and rotate transforms."""
    import os, math, xml.etree.ElementTree as ET
    from reportlab.graphics.shapes import Path, Group, Ellipse
    svg = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "icon.svg")
    root = ET.parse(svg).getroot(); K = 15.0 / 128.0 * mm * 1.0
    d = Drawing(15 * mm, 15 * mm); top = Group(); top.transform = (K, 0, 0, -K, 0, 15 * mm); d.add(top)
    col = lambda v: None if v in (None, "none") else colors.HexColor(v)
    def style(a, o):
        f, sk = a.get("fill"), a.get("stroke")
        o.fillColor = col(f) if f else (None if sk else colors.black); o.strokeColor = col(sk)
        o.strokeWidth = float(a.get("stroke-width", 1)); o.strokeLineCap = 1 if a.get("stroke-linecap") == "round" else 0; o.strokeLineJoin = 1 if a.get("stroke-linejoin") == "round" else 0
        if a.get("stroke-dasharray"): o.strokeDashArray = [float(x) for x in a.get("stroke-dasharray").split()]
        if a.get("opacity"):
            op = float(a.get("opacity")); o.fillOpacity = op; o.strokeOpacity = op
        return o
    def path(dstr):
        toks = re.findall(r"[A-Za-z]|-?\d*\.?\d+", dstr); i = 0; cmd = None; cur = (0.0, 0.0); start = cur; lastq = None; lastc = None
        P = Path(); P.points = []; P.operators = []
        def num():
            nonlocal i; v = float(toks[i]); i += 1; return v
        while i < len(toks):
            if toks[i].isalpha(): cmd = toks[i]; i += 1
            rel = cmd.islower(); c = cmd.upper(); ox, oy = cur if rel else (0.0, 0.0)
            if c == "M":
                cur = (ox + num(), oy + num()); start = cur; P.moveTo(*cur); cmd = "l" if rel else "L"; lastq = lastc = None
            elif c == "L":
                cur = (ox + num(), oy + num()); P.lineTo(*cur); lastq = lastc = None
            elif c == "H":
                cur = (ox + num() if rel else num(), cur[1]); P.lineTo(*cur); lastq = lastc = None
            elif c == "V":
                cur = (cur[0], oy + num() if rel else num()); P.lineTo(*cur); lastq = lastc = None
            elif c in "QT":
                if c == "Q": q = (ox + num(), oy + num())
                else: q = (2 * cur[0] - lastq[0], 2 * cur[1] - lastq[1]) if lastq else cur
                en = (ox + num(), oy + num())
                P.curveTo(cur[0] + 2 / 3 * (q[0] - cur[0]), cur[1] + 2 / 3 * (q[1] - cur[1]), en[0] + 2 / 3 * (q[0] - en[0]), en[1] + 2 / 3 * (q[1] - en[1]), *en); cur = en; lastq = q; lastc = None
            elif c in "CS":
                if c == "C": c1 = (ox + num(), oy + num())
                else: c1 = (2 * cur[0] - lastc[0], 2 * cur[1] - lastc[1]) if lastc else cur
                c2 = (ox + num(), oy + num()); en = (ox + num(), oy + num())
                P.curveTo(c1[0], c1[1], c2[0], c2[1], *en); cur = en; lastc = c2; lastq = None
            elif c == "Z": P.closePath(); cur = start; lastq = lastc = None
            else: i += 1
        return P
    def walk(e, parent, inh):
        for ch in e:
            t = ch.tag.split("}")[1]; a = dict(inh); a.update(ch.attrib)
            if t == "g":
                sub = Group(); parent.add(sub); walk(ch, sub, {k: v for k, v in a.items() if k in ("fill", "stroke", "stroke-width", "stroke-linecap", "stroke-linejoin", "opacity")}); continue
            if t == "rect":
                o = Rect(float(a.get("x", 0)), float(a.get("y", 0)), float(a.get("width")), float(a.get("height")), rx=float(a.get("rx", 0)), ry=float(a.get("rx", 0)))
            elif t == "circle": o = Circle(float(a.get("cx")), float(a.get("cy")), float(a.get("r")))
            elif t == "ellipse": o = Ellipse(float(a.get("cx")), float(a.get("cy")), float(a.get("rx")), float(a.get("ry")))
            elif t == "path": o = path(a.get("d"))
            else: continue
            style(a, o); tr = a.get("transform"); m = re.match(r"rotate\(\s*(-?[\d.]+)[ ,]+(-?[\d.]+)[ ,]+(-?[\d.]+)\s*\)", tr or "")
            if m:
                ang, cx, cy = (float(m.group(k)) for k in (1, 2, 3)); cs, sn = math.cos(math.radians(ang)), math.sin(math.radians(ang))
                wrap = Group(o); wrap.transform = (cs, sn, -sn, cs, cx - cx * cs + cy * sn, cy - cx * sn - cy * cs); parent.add(wrap)
            else: parent.add(o)
    walk(root, top, {})
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
