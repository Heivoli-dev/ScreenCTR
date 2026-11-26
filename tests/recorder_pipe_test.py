#!/usr/bin/env python3
"""Pipe raw BGRA frames into ffmpeg with exactly the arguments used by
pc/src/Recorder.cpp, then check the MP4 with ffprobe."""
import json, re, subprocess, sys, pathlib

out = sys.argv[1] if len(sys.argv) > 1 else "rec_test.mp4"
src = (pathlib.Path(__file__).resolve().parent.parent / "pc/src/Recorder.cpp").read_text()
block = src[src.index("QStringList args = {"):src.index("};", src.index("QStringList args = {"))]
tokens = re.findall(r'"([^"]*)"', block)
w, h, fps, n = 800, 960, 30, 45
args = []
for t in tokens:
    if t == "%1x%2": t = f"{w}x{h}"
    args.append(t)
# -r <fps> is built with QString::number(fps); the output path is the last arg
args.insert(args.index("-r") + 1, str(fps))
args.append(out)
p = subprocess.Popen(["ffmpeg"] + args, stdin=subprocess.PIPE)
frame = bytearray(w * h * 4)
for i in range(n):
    frame[(i * 4) % len(frame)] = 255
    p.stdin.write(frame)
p.stdin.close()
rc = p.wait()
info = json.loads(subprocess.check_output(["ffprobe", "-v", "error", "-print_format", "json", "-show_streams", out]))
s = info["streams"][0]
ok = rc == 0 and s["codec_name"] == "h264" and s["width"] == w and s["height"] == h and s["pix_fmt"] == "yuv420p" and int(s["nb_frames"]) == n
print(f"{'OK  ' if ok else 'FAIL'} ffmpeg {' '.join(args[:-1])} -> {s['codec_name']} {s['width']}x{s['height']} {s['nb_frames']} images")
sys.exit(0 if ok else 1)
