#!/usr/bin/env python3
"""Builds the Vita LiveArea images from artwork you supply (not kept in git).

usage: make_livearea.py ICON_ART BACKGROUND_ART OUT_DIR

ICON_ART is drawn centred on a sky-blue bubble (icon0.png, 128x128).
BACKGROUND_ART is cropped to the LiveArea background (bg.png, 840x500) and,
around its upper right, to the start gate (startup.png, 280x158).
All PNGs are written as 8-bit palette images, as the Vita expects.
"""
import sys
from pathlib import Path
from PIL import Image

def save_indexed(img, path):
    img.convert("RGB").quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(path, optimize=True)

def crop_to(img, aspect, cx, cy, w=None):
    W, H = img.size
    if w is None:
        w = min(W, int(H * aspect))
    h = int(w / aspect)
    h = min(h, H); w = int(h * aspect)
    x = min(max(0, int(cx - w / 2)), W - w)
    y = min(max(0, int(cy - h / 2)), H - h)
    return img.crop((x, y, x + w, y + h))

def main():
    icon_art, bg_art, out = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
    out.mkdir(parents=True, exist_ok=True)

    # icon0: vertical sky gradient + the art fitted into 112x112.
    icon = Image.new("RGBA", (128, 128))
    top, bottom = (120, 200, 245), (40, 120, 200)
    for y in range(128):
        t = y / 127
        c = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,)
        for x in range(128):
            icon.putpixel((x, y), c)
    art = Image.open(icon_art).convert("RGBA")
    art.thumbnail((112, 112), Image.Resampling.LANCZOS)
    icon.alpha_composite(art, ((128 - art.width) // 2, (128 - art.height) // 2))
    save_indexed(icon, out / "icon0.png")

    bg = Image.open(bg_art).convert("RGB")
    W, H = bg.size
    # Keep the right side (logo and Link) in the background.
    save_indexed(crop_to(bg, 840 / 500, W * 0.58, H / 2).resize((840, 500), Image.Resampling.LANCZOS), out / "bg.png")
    # Start gate: the logo area (upper right of the key art).
    save_indexed(crop_to(bg, 280 / 158, W * 0.735, H * 0.33, w=int(W * 0.47)).resize((280, 158), Image.Resampling.LANCZOS),
                 out / "startup.png")

    (out / "template.xml").write_text(
        '<?xml version="1.0" encoding="utf-8"?>\n'
        '<livearea style="a1" format-ver="01.00" content-rev="1">\n'
        '  <livearea-background>\n    <image>bg.png</image>\n  </livearea-background>\n'
        '  <gate>\n    <startup-image>startup.png</startup-image>\n  </gate>\n'
        '</livearea>\n')

if __name__ == "__main__":
    main()
