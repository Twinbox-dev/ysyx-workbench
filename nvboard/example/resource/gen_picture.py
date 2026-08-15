import sys
import os
from PIL import Image

VGA_W = 640
VGA_H = 480
VGA_ROW = 1024


def default_out():
    home = os.path.expanduser("~")
    for cand in (os.path.join(home, "Desktop"), os.path.join(home, "OneDrive", "Desktop")):
        if os.path.isdir(cand):
            return os.path.join(cand, "picture.hex")
    return os.path.join(os.getcwd(), "picture.hex")


def main():
    if len(sys.argv) < 2:
        print("usage: python gen_picture.py <image> [out.hex]")
        sys.exit(1)

    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else default_out()

    img = Image.open(src).convert("RGB").resize((VGA_W, VGA_H))

    with open(out, "w", encoding="ascii") as f:
        for y in range(VGA_H):
            for x in range(VGA_W):
                r, g, b = img.getpixel((x, y))
                f.write("%02X%02X%02X\n" % (r, g, b))
            for _ in range(VGA_ROW - VGA_W):
                f.write("000000\n")

    print("written:", out, "lines:", VGA_H * VGA_ROW)


if __name__ == "__main__":
    main()