#!/usr/bin/env python3
"""Decode the JPEGs produced by test_common with an independent decoder
(Pillow / libjpeg) and compare them with the reference images.

Two criteria per file:
  * luminance PSNR above a floor (catches DCT / Huffman / quantizer bugs);
  * RGB PSNR within 1 dB of what libjpeg itself achieves with the same
    quality and subsampling (4:2:0 legitimately loses colour detail)."""
import io, math, sys, pathlib
from PIL import Image

d = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".")

def psnr(a, b):
    pa, pb = a.tobytes(), b.tobytes()
    mse = sum((x - y) ** 2 for x, y in zip(pa, pb)) / len(pa)
    return 99.0 if mse == 0 else 10 * math.log10(255 * 255 / mse)

cases = [  # file, reference, quality, subsampling (Pillow: 0=4:4:4, 2=4:2:0), luma floor
    ("enc_q30_420.jpg", "ref_400x240.ppm", 30, 2, 34), ("enc_q70_420.jpg", "ref_400x240.ppm", 70, 2, 40),
    ("enc_q90_420.jpg", "ref_400x240.ppm", 90, 2, 45), ("enc_q30_444.jpg", "ref_400x240.ppm", 30, 0, 34),
    ("enc_q70_444.jpg", "ref_400x240.ppm", 70, 0, 40), ("enc_q90_444.jpg", "ref_400x240.ppm", 90, 0, 45),
    ("enc_bottom_q70_420.jpg", "ref_320x240.ppm", 70, 2, 40), ("enc_odd_q70_420.jpg", "ref_37x21.ppm", 70, 2, 40),
]
fail = 0
for f, ref, q, ss, floor in cases:
    img = Image.open(d / f); img.load()
    r = Image.open(d / ref).convert("RGB")
    ok = img.size == r.size and img.format == "JPEG"
    mine = img.convert("RGB")
    buf = io.BytesIO(); r.save(buf, "JPEG", quality=q, subsampling=ss)
    lib = Image.open(io.BytesIO(buf.getvalue())).convert("RGB")
    p_rgb, p_lib = psnr(mine, r), psnr(lib, r)
    p_luma = psnr(mine.convert("L"), r.convert("L"))
    ok = ok and p_luma >= floor and p_rgb >= p_lib - 1.0
    print(f"{'OK  ' if ok else 'FAIL'} {f}: {img.size} luma {p_luma:.1f} dB (min {floor}), "
          f"RGB {p_rgb:.1f} dB vs libjpeg {p_lib:.1f} dB, {len((d / f).read_bytes())} B vs {len(buf.getvalue())} B")
    fail += not ok
sys.exit(1 if fail else 0)
