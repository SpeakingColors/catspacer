"""Builds the interface-tour pictures and docs/tour/<key>.json from the REAL compiled plugin window.
Usage: python3 tools/tour_from_editor.py <key> <screenshot.png> <dump.json> <docs/tour dir> [preset name]
The dump comes from running the standalone with env CAT_TOUR_DUMP=<file> (see FxEditor::dumpTour).
The screenshot is a full-screen capture of the standalone window; the editor area is found automatically."""
import json, os, sys
from PIL import Image, ImageDraw, ImageFont
key, shot, dump, tdir = sys.argv[1:5]
preset = sys.argv[5] if len(sys.argv) > 5 else ""
d = json.load(open(dump)); im = Image.open(shot).convert("RGB")
W, H = d["w"], d["h"]
# find the editor: wood cheeks start at the first non-black column; top is the first row after the yellow audio bar
px = im.load(); sw, sh = im.size
y0 = sh // 2
x0 = next(x for x in range(sw) if sum(px[x, y0]) > 60)
def is_bar(c): return c[0] > 235 and c[1] > 235 and 150 < c[2] < 235
ytop = next(y for y in range(sh) if is_bar(px[x0 + 40, y]))
ybot = ytop
while is_bar(px[x0 + 40, ybot]): ybot += 1
ox, oy = x0, ybot
# the dump is in editor pixels; the capture may be scaled (editor resized): derive the scale from the width
right = max(x for x in range(sw) if sum(px[x, y0]) > 60) + 1
sc = (right - x0) / W
sc = 1.0 if sc < 1.3 else round(sc * 4) / 4
edit = im.crop((ox, oy, ox + round(W * sc), oy + round(H * sc)))
def S(b): return [b["x"] * sc, b["y"] * sc, b["w"] * sc, b["h"] * sc]
try: F = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", max(12, round(15 * sc)))
except Exception: F = ImageFont.load_default()
PINK, TEAL = (255, 62, 150), (24, 200, 190)
def badge(dr, x, y, txt, col, sq=False):
    r = round(11 * sc); box = [x - r, y - r, x + r, y + r]
    (dr.rectangle if sq else dr.ellipse)(box, fill=col, outline=(255, 255, 255), width=max(1, round(sc)))
    tw = dr.textlength(txt, font=F); dr.text((x - tw / 2, y - r * 0.72), txt, font=F, fill=(255, 255, 255))
full = edit.copy(); dr = ImageDraw.Draw(full)
n = 0; top = []; panels = []
for t in sorted(d["top"], key=lambda t: t["box"]["x"]):
    n += 1; b = S(t["box"]); badge(dr, b[0] + 4 * sc, b[1] + 4 * sc, str(n), PINK)
    nm = {"Presets": "Preset menu", "Info bubbles": "Info bubbles (i)"}.get(t["name"], t["name"])
    top.append({"n": n, "name": nm, "tip": t["tip"], "value": t["value"], "kind": t["kind"]})
letters = "ABCDEFGH"; names = {}
for pi, p in enumerate(d["panels"]):
    L = letters[pi]; pb = S(p["box"]); badge(dr, pb[0] + pb[2] - 16 * sc, pb[1] + 14 * sc, L, TEAL, sq=True)
    items = []
    for it in p["items"]:
        n += 1; b = S(it["box"]); badge(dr, b[0] + 2 * sc, b[1] + 2 * sc, str(n), PINK)
        nm = it["name"]
        if it["kind"] == "button" and nm == p["title"]: nm = "On"
        if it["kind"] == "menu" and nm == "Type": names[str(n)] = p["title"] + " type"
        items.append({"n": n, "kind": it["kind"], "name": nm, "tip": it["tip"], "value": it["value"], "box": it["box"]})
    panels.append({"title": p["title"], "box": p["box"], "img": "%s_p%d.png" % (key, pi + 1), "disp": {"letter": L, "box": p["display"]}, "items": items})
full.save(os.path.join(tdir, key + "_full.png"))
clean = edit.resize((1200, round(1200 * edit.size[1] / edit.size[0])), Image.LANCZOS); clean.save(os.path.join(tdir, "..", "screenshot.png"))
for pi, p in enumerate(panels):
    b = S(p["box"]); pad = round(4 * sc)
    full.crop((max(0, round(b[0]) - pad), max(0, round(b[1]) - pad), round(b[0] + b[2]) + pad, round(b[1] + b[3]) + pad)).save(os.path.join(tdir, p["img"]))
json.dump({"key": key, "preset": preset, "box": [0, 0, W, H], "items": panels, "displays": {}, "top": top, "names": names}, open(os.path.join(tdir, key + ".json"), "w"), indent=1)
print("ok", key, "scale", round(sc, 2), "items", n)
