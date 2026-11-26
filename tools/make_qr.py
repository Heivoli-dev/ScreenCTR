#!/usr/bin/env python3
"""Make a QR code PNG for FBI's "Remote Install > Scan QR Code".
usage: make_qr.py <url> <out.png>   (needs: pip install qrcode[pil])"""
import sys
import qrcode
from PIL import ImageDraw, ImageFont

url, out = sys.argv[1], sys.argv[2]
qr = qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_M, box_size=10, border=4)
qr.add_data(url)
qr.make(fit=True)
img = qr.make_image(fill_color="black", back_color="white").convert("RGB")
# Caption under the code
w, h = img.size
canvas = img.resize((w, h))
from PIL import Image
final = Image.new("RGB", (w, h + 60), "white")
final.paste(canvas, (0, 0))
d = ImageDraw.Draw(final)
try:
    font = ImageFont.truetype("DejaVuSans-Bold.ttf", 22)
except OSError:
    font = ImageFont.load_default()
text = "ScreenCTR.cia (FBI)"
tw = d.textlength(text, font=font)
d.text(((w - tw) / 2, h + 15), text, fill="black", font=font)
final.save(out)
print("QR ->", out, "for", url)
