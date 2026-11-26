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

# HOME Menu banner (256x128, shown above the icon) + silent banner sound
from PIL import ImageFont
ban = Image.new("RGBA", (256, 128), BG + (255,))
ban.paste(icon(96), (16, 16), icon(96))
bd = ImageDraw.Draw(ban)
try:
    f1 = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 22)
    f2 = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 11)
except OSError:
    f1 = f2 = ImageFont.load_default()
bd.text((122, 36), "Screen", fill=FG, font=f1)
bd.text((122 + bd.textlength("Screen", font=f1), 36), "CTR", fill=ACC, font=f1)
bd.text((122, 68), "Two screens.", fill=(138, 147, 166), font=f2)
bd.text((122, 83), "One connection.", fill=(138, 147, 166), font=f2)
ban.save(OUT / "banner.png")
import wave
with wave.open(str(OUT / "banner_silence.wav"), "wb") as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(32728); w.writeframes(b"\0\0\0\0" * 3272)
print("assets written to", OUT)
