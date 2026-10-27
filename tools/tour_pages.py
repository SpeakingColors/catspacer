"""Adds annotated interface-tour pages to a Studio 87 plugin manual.
Used by tools/make_manual.py when docs/tour/<key>.json and extra.json exist."""
import json, os
from reportlab.lib import colors
from reportlab.lib.units import mm
from reportlab.lib.utils import ImageReader
from reportlab.platypus import Paragraph, Spacer, Table, TableStyle, Image, PageBreak, KeepTogether, CondPageBreak

def _img(path, width, maxh=None):
    w, h = ImageReader(path).getSize()
    if maxh and width * h / w > maxh: width = maxh * w / h
    return Image(path, width=width, height=width * h / w)

def tour_story(tdir, key, H2, B, CELL, CELLB, SM, TEAL, PANEL, esc, st):
    p = os.path.join(tdir, key + ".json")
    if not os.path.exists(p): return []
    d = json.load(open(p)); X = json.load(open(os.path.join(tdir, "extra.json")))[key]
    W = 170 * mm; out = [PageBreak(), Paragraph("Interface tour", H2),
        Paragraph(esc("Pink numbers mark controls, teal letters mark displays. Every number is explained in the tables on the next pages. The picture is a screenshot of the real compiled plugin window."), B),
        Spacer(1, 4), _img(os.path.join(tdir, key + "_full.png"), W), Spacer(1, 6)]
    out.append(Paragraph("Signal flow", st("sf", fontSize=11.5, leading=15, textColor=TEAL, font="Helvetica-Bold", spaceBefore=6, spaceAfter=3)))
    out.append(Paragraph(esc(X["flow"]), B))
    names = d["names"] if "names" in d else X.get("names", {})
    hdr = [[Paragraph("<b>No.</b>", CELL), Paragraph("<b>Control</b>", CELL), Paragraph("<b>What it does</b>", CELL)]]
    rows = [[Paragraph(str(t["n"]), CELLB), Paragraph(esc(t["name"]), CELLB), Paragraph(esc(t["tip"]), CELL)] for t in d["top"]]
    def tbl(r):
        t = Table(r, colWidths=[10 * mm, 34 * mm, 126 * mm]); t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("BACKGROUND", (0, 0), (-1, -1), PANEL), ("LINEBELOW", (0, 0), (-1, -1), 0.5, colors.HexColor("#161638")), ("TOPPADDING", (0, 0), (-1, -1), 4), ("BOTTOMPADDING", (0, 0), (-1, -1), 4)])); return t
    hs = st("th", fontSize=11.5, leading=15, textColor=TEAL, font="Helvetica-Bold", spaceBefore=6, spaceAfter=3)
    out += [Paragraph("Header bar", hs), tbl(rows)]
    for P in d["items"]:
        L = P["disp"]["letter"]
        blk = [CondPageBreak(150 * mm), Paragraph(esc("%s. %s" % (L, P["title"])), H2), _img(os.path.join(tdir, P["img"]), 120 * mm, 62 * mm), Spacer(1, 4),
               Paragraph("<b>Display %s:</b> %s" % (L, esc(X["displays"][L])), B), Spacer(1, 4)]
        rs = []
        for it in P["items"]:
            nm = names.get(str(it["n"]), it["name"]); val = (" (shown: %s)" % it["value"]) if it["value"] and it["kind"] != "button" else ""
            rs.append([Paragraph(str(it["n"]), CELLB), Paragraph(esc(nm), CELLB), Paragraph(esc(it["tip"]) + "<font color='#8a8ab0'>" + esc(val) + "</font>", CELL)])
        blk.append(tbl(rs)); out += blk
    out += [PageBreak(), Paragraph("Under the hood", H2)]
    for t in X["deep"]: out.append(Paragraph("&bull; " + esc(t), B))
    return out
