#!/usr/bin/env python3
"""Turn the emulator's .ppm screens into 3x PNGs inside a dark Cardputer-style frame, with the RGB LED lit."""
import glob, os, sys
from PIL import Image, ImageDraw

src, dst = sys.argv[1], sys.argv[2]
os.makedirs(dst, exist_ok=True)
S = 3
for ppm in sorted(glob.glob(os.path.join(src, "*.ppm"))):
    name = os.path.splitext(os.path.basename(ppm))[0]
    scr = Image.open(ppm).convert("RGB").resize((240 * S, 135 * S), Image.NEAREST)
    r, g, b = map(int, open(os.path.join(src, name + ".led")).read().split())
    led = tuple(min(255, v * 4) for v in (r, g, b)) if (r or g or b) else (40, 40, 40)
    pad = 24
    frame = Image.new("RGB", (scr.width + 2 * pad, scr.height + 2 * pad + 20), (255, 255, 255))
    d = ImageDraw.Draw(frame)
    d.rounded_rectangle((0, 0, frame.width - 1, frame.height - 1), 22, fill=(60, 62, 70), outline=(35, 36, 42), width=4)
    frame.paste(scr, (pad, pad))
    cx, cy = frame.width - 30, frame.height - 22
    d.ellipse((cx - 7, cy - 7, cx + 7, cy + 7), fill=led)
    frame.save(os.path.join(dst, "cardputer-" + name + ".png"))
