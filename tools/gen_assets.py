#!/usr/bin/env python3
"""Generate ScreenCTR graphic assets (original design, Heivoli Network):
two stacked rounded screens joined by a link, green accent on dark."""
import pathlib
from PIL import Image, ImageDraw

OUT = pathlib.Path(__file__).resolve().parent.parent / "assets"
OUT.mkdir(exist_ok=True)
BG, ACC, FG, DIM = (11, 13, 18), (61, 220, 151), (232, 236, 242), (38, 44, 58)

def icon(size):
    s = size / 256
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([0, 0, size - 1, size - 1], radius=int(56 * s), fill=BG)
    # top screen (5:3) and bottom screen (4:3)
    d.rounded_rectangle([int(38 * s), int(34 * s), int(218 * s), int(122 * s)], radius=int(12 * s), fill=DIM, outline=ACC, width=max(1, int(8 * s)))
    d.rounded_rectangle([int(62 * s), int(140 * s), int(194 * s), int(226 * s)], radius=int(12 * s), fill=DIM, outline=FG, width=max(1, int(8 * s)))
    # connection: three dots
    for i, x in enumerate((104, 128, 152)):
        r = int(7 * s)
        d.ellipse([int(x * s) - r, int(131 * s) - r, int(x * s) + r, int(131 * s) + r], fill=ACC if i != 1 else FG)
    return im

big = icon(256)
big.save(OUT / "icon256.png")
icon(48).save(OUT / "icon48.png")
big.save(OUT / "screenctr.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])

svg = f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256">
  <rect width="256" height="256" rx="56" fill="rgb{BG}"/>
  <rect x="38" y="34" width="180" height="88" rx="12" fill="rgb{DIM}" stroke="rgb{ACC}" stroke-width="8"/>
  <rect x="62" y="140" width="132" height="86" rx="12" fill="rgb{DIM}" stroke="rgb{FG}" stroke-width="8"/>
  <circle cx="104" cy="131" r="7" fill="rgb{ACC}"/><circle cx="128" cy="131" r="7" fill="rgb{FG}"/><circle cx="152" cy="131" r="7" fill="rgb{ACC}"/>
</svg>
"""
(OUT / "logo.svg").write_text(svg)
print("assets written to", OUT)
