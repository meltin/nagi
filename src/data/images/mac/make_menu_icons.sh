#!/bin/zsh
# Regenerates Nagi's menu bar icons (the six input mode .tiff files next to this script).
# Each is a black pill the size of the system "ABC" icon (22x15pt) with the glyph knocked
# out. They are template images: macOS recolors them for the menu bar.
# Usage: zsh make_menu_icons.sh   (needs uvx; downloads Pillow on first use)
set -eu
here="${0:A:h}"
tmp="$(mktemp -d)"
uvx --from pillow python - "$tmp" <<'PY'
import sys
from PIL import Image, ImageChops, ImageDraw, ImageFont

out = sys.argv[1]
FONT = '/System/Library/Fonts/ヒラギノ角ゴシック W6.ttc'
GLYPHS = {'hiragana': 'あ', 'full_katakana': 'ア', 'half_katakana': 'ｱ',
          'full_ascii': 'Ａ', 'half_ascii': 'A', 'direct': 'A'}
W, H, SHAPE_H, RADIUS, GLYPH = 22, 16, 15, 4, 11.5  # points


def draw(ch, scale):
    k = scale * 8  # supersample, then shrink
    rect = Image.new('L', (W * k, H * k), 0)
    top = (H - SHAPE_H) * k // 2
    ImageDraw.Draw(rect).rounded_rectangle(
        (0, top, W * k - 1, top + SHAPE_H * k - 1), RADIUS * k, fill=255)
    glyph = Image.new('L', rect.size, 0)
    font = ImageFont.truetype(FONT, int(GLYPH * k))
    d = ImageDraw.Draw(glyph)
    l, t, r, b = d.textbbox((0, 0), ch, font=font)
    d.text(((W * k - (r - l)) / 2 - l, (H * k - (b - t)) / 2 - t), ch, font=font, fill=255)
    alpha = ImageChops.subtract(rect, glyph).resize((W * scale, H * scale), Image.LANCZOS)
    image = Image.new('RGBA', alpha.size, (0, 0, 0, 0))
    image.putalpha(alpha)
    return image


for name, ch in GLYPHS.items():
    for scale in (1, 2):
        draw(ch, scale).save(f'{out}/{name}@{scale}x.png', dpi=(72 * scale, 72 * scale))
PY
for n in hiragana full_katakana half_katakana full_ascii half_ascii direct; do
  tiffutil -cathidpicheck "$tmp/$n@1x.png" "$tmp/$n@2x.png" -out "$here/$n.tiff"
done
