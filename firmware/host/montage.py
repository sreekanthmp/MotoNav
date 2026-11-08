"""Review helper: tiles the rendered PBM screens into PNG contact sheets.

Not part of the tests. Needs Pillow. Usage: python3 -I montage.py out
Writes out/montage_<WxH>.png at 4x scale with the file name under each tile.
"""
import glob
import os
import sys

from PIL import Image, ImageDraw

SCALE = 4
COLS = 4
PAD = 10
LABEL_H = 14


def load_pbm(path):
    with open(path) as f:
        tokens = f.read().split()
    assert tokens[0] == "P1"
    w, h = int(tokens[1]), int(tokens[2])
    bits = "".join(tokens[3:])
    img = Image.new("RGB", (w, h), (0, 0, 0))
    px = img.load()
    for i, ch in enumerate(bits[: w * h]):
        if ch == "1":
            px[i % w, i // w] = (120, 220, 255)  # OLED-ish blue-white
    return img.resize((w * SCALE, h * SCALE), Image.NEAREST)


def main(out_dir):
    for size in sorted({os.path.basename(p).split("_")[0] for p in glob.glob(os.path.join(out_dir, "*.pbm"))}):
        files = sorted(glob.glob(os.path.join(out_dir, size + "_*.pbm")))
        tiles = [(os.path.basename(f)[len(size) + 1 : -4], load_pbm(f)) for f in files]
        tw, th = tiles[0][1].size
        rows = (len(tiles) + COLS - 1) // COLS
        sheet = Image.new("RGB", (COLS * (tw + PAD) + PAD, rows * (th + LABEL_H + PAD) + PAD), (30, 30, 30))
        d = ImageDraw.Draw(sheet)
        for i, (name, img) in enumerate(tiles):
            x = PAD + (i % COLS) * (tw + PAD)
            y = PAD + (i // COLS) * (th + LABEL_H + PAD)
            d.text((x, y), name, fill=(220, 220, 220))
            sheet.paste(img, (x, y + LABEL_H))
        out = os.path.join(out_dir, "montage_%s.png" % size)
        sheet.save(out)
        print("wrote", out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "out")
