#!/usr/bin/env python3
"""Crop a full-body companion PNG into the 52x64 portrait Focus Wallet embeds.

drawCompanion() draws a head-and-shoulders portrait, not the full body. The
crop matches the framing already used for assets/focus-wallet/sample.png:
horns, hair, and face stay inside the frame, with a small margin above the
horns and the portrait ending around the shoulders.

Near-black pixels connected to the image edge become transparent so the
character sits on the Cardputer UI. Dark horns and clothing that are enclosed
by the character stay opaque.

Requires Pillow (pip install pillow).

Example:
  python3 scripts/embed_companion.py \\
    assets/focus-wallet/character-happy.png \\
    -o src/focus_wallet/character_happy_image.h \\
    --symbol character_happy_png
"""

from __future__ import annotations

import argparse
import io
from pathlib import Path

from PIL import Image

# Measured from the portrait already embedded in character_image.h.
# sample.png is 1122x1402. Its character bounds are (191, 51)-(906, 1347),
# and that portrait was a 760x935 crop at (171, 25) resized to 52x64.
PORTRAIT_W = 52
PORTRAIT_H = 64
PAD_TOP_RATIO = 26 / 1297
CROP_HEIGHT_RATIO = 935 / 1297
CONTENT_THRESHOLD = 12
KEY_THRESHOLD = 18
KEY_SOFTNESS = 14


def content_bbox(image: Image.Image) -> tuple[int, int, int, int]:
    image = image.convert("RGBA")
    pixels = image.load()
    width, height = image.size
    min_x, min_y, max_x, max_y = width, height, -1, -1
    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = pixels[x, y]
            if alpha > 16 and max(red, green, blue) > CONTENT_THRESHOLD:
                min_x = min(min_x, x)
                min_y = min(min_y, y)
                max_x = max(max_x, x)
                max_y = max(max_y, y)
    if max_x < 0:
        raise SystemExit("no character pixels found (image looks empty)")
    return min_x, min_y, max_x, max_y


def crop_portrait(image: Image.Image) -> Image.Image:
    """Return a 52x64 RGBA portrait with the backdrop removed."""
    image = image.convert("RGBA")
    x0, y0, x1, y1 = content_bbox(image)
    content_h = y1 - y0 + 1
    pad_top = round(PAD_TOP_RATIO * content_h)
    crop_h = max(1, round(CROP_HEIGHT_RATIO * content_h))
    crop_w = max(1, round(crop_h * PORTRAIT_W / PORTRAIT_H))
    center_x = (x0 + x1) / 2
    left = int(round(center_x - crop_w / 2))
    top = y0 - pad_top
    left = max(0, min(left, max(0, image.width - crop_w)))
    top = max(0, min(top, max(0, image.height - crop_h)))
    right = min(image.width, left + crop_w)
    bottom = min(image.height, top + crop_h)
    cropped = image.crop((left, top, right, bottom))
    resized = cropped.resize((PORTRAIT_W, PORTRAIT_H), Image.Resampling.BICUBIC)
    return key_background(resized)


def key_background(image: Image.Image) -> Image.Image:
    """Clear near-black pixels that touch the portrait edge."""
    image = image.convert("RGBA")
    pixels = image.load()
    width, height = image.size
    candidate = [[False] * width for _ in range(height)]
    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = pixels[x, y]
            if alpha < 8 or max(red, green, blue) <= KEY_THRESHOLD:
                candidate[y][x] = True

    background = [[False] * width for _ in range(height)]
    queue: list[tuple[int, int]] = []
    for x in range(width):
        for y in (0, height - 1):
            if candidate[y][x]:
                background[y][x] = True
                queue.append((x, y))
    for y in range(height):
        for x in (0, width - 1):
            if candidate[y][x] and not background[y][x]:
                background[y][x] = True
                queue.append((x, y))
    while queue:
        x, y = queue.pop()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < width and 0 <= ny < height and candidate[ny][nx] and not background[ny][nx]:
                background[ny][nx] = True
                queue.append((nx, ny))

    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = pixels[x, y]
            if background[y][x]:
                pixels[x, y] = (red, green, blue, 0)
                continue
            touches_background = False
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < width and 0 <= ny < height and background[ny][nx]:
                    touches_background = True
                    break
            if touches_background:
                fade = int((max(red, green, blue) - KEY_THRESHOLD) * 255 / KEY_SOFTNESS)
                fade = max(0, min(255, fade))
                pixels[x, y] = (red, green, blue, min(alpha, fade))
    return image


def png_bytes(image: Image.Image) -> bytes:
    buffer = io.BytesIO()
    image.save(buffer, format="PNG", optimize=True)
    return buffer.getvalue()


def c_source(data: bytes, symbol: str, source_name: str) -> str:
    lines = [
        "#pragma once",
        "#include <Arduino.h>",
        "",
        f"// Companion portrait ({PORTRAIT_W}x{PORTRAIT_H}) for drawCompanion().",
        f"// Cropped from {source_name} by scripts/embed_companion.py.",
        "// Horns, hair, and face stay inside the frame. Edge-connected black is transparent.",
        f"const uint8_t {symbol}[] PROGMEM = {{",
    ]
    for offset in range(0, len(data), 12):
        chunk = data[offset : offset + 12]
        body = ", ".join(f"0x{byte:02x}" for byte in chunk)
        if offset + 12 < len(data):
            body += ","
        lines.append(f"  {body}")
    lines.append("};")
    lines.append(f"const uint32_t {symbol}_len = {len(data)};")
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("png", type=Path, help="full-body source PNG")
    parser.add_argument("-o", "--output", type=Path, required=True, help="header file to write")
    parser.add_argument("--symbol", default="character_happy_png")
    parser.add_argument("--preview", type=Path, help="optional 52x64 PNG to write as well")
    args = parser.parse_args()

    portrait = crop_portrait(Image.open(args.png))
    data = png_bytes(portrait)
    source_name = args.png.as_posix()
    args.output.write_text(c_source(data, args.symbol, source_name), encoding="utf-8")
    if args.preview:
        args.preview.parent.mkdir(parents=True, exist_ok=True)
        args.preview.write_bytes(data)
    print(f"wrote {args.output} ({len(data)} bytes, {PORTRAIT_W}x{PORTRAIT_H})")


if __name__ == "__main__":
    main()
