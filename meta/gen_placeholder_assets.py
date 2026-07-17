"""
Regenerates meta/icon.png, meta/banner.png, and meta/audio.wav straight from
the Sprout Lands asset pack, so the game ships a themed icon/banner instead
of a blank placeholder without needing any custom branding.

icon.png:   48x48   (bannertool's required SMDH icon size)
banner.png: 256x128 (bannertool's required CIA banner size, RGBA - matches
            the sibling homeassist-ds project's working banner.png format)
audio.wav:  1s of silence (bannertool needs *some* audio file even though
            the game currently has no sound)

Re-run with: python meta/gen_placeholder_assets.py
Requires: Pillow (`pip install pillow`)
"""
import struct
import wave
import zipfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
SPROUT_ZIP = Path.home() / "Downloads" / "Sprout Lands - Sprites - Basic pack.zip"
PACK = "Sprout Lands - Sprites - Basic pack"

GRASS_BG = (127, 166, 80, 255)


def write_silent_wav(path, seconds=1, sample_rate=32728):
    nframes = seconds * sample_rate
    silence = struct.pack("<h", 0) * 2 * nframes
    with wave.open(str(path), "w") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(sample_rate)
        w.writeframes(silence)


def load_tile(zf, sheet, x, y, w=16, h=16):
    with zf.open(f"{PACK}/{sheet}") as f:
        img = Image.open(f).convert("RGBA")
        img.load()
    return img.crop((x, y, x + w, y + h))


def paste_scaled(base, tile, size, pos):
    big = tile.resize(size, Image.NEAREST)
    base.paste(big, pos, big)


def make_icon(zf):
    player = load_tile(zf, "Characters/Basic Charakter Spritesheet.png", 16, 16)
    icon = Image.new("RGBA", (48, 48), GRASS_BG)
    paste_scaled(icon, player, (36, 36), (6, 6))
    icon.save(HERE / "icon.png")


def make_banner(zf):
    grass = load_tile(zf, "Tilesets/Grass.png", 0, 80)
    tilled = load_tile(zf, "Tilesets/Tilled Dirt.png", 0, 80)
    wheat = load_tile(zf, "Objects/Basic Plants.png", 64, 0)
    turnip = load_tile(zf, "Objects/Basic Plants.png", 64, 16)
    player = load_tile(zf, "Characters/Basic Charakter Spritesheet.png", 16, 16)

    banner = Image.new("RGBA", (256, 128), GRASS_BG)
    for gx in range(0, 256, 32):
        for gy in range(0, 96, 32):
            paste_scaled(banner, grass, (32, 32), (gx, gy))
    for gx in range(0, 256, 32):
        paste_scaled(banner, tilled, (32, 32), (gx, 96))

    paste_scaled(banner, wheat, (48, 48), (60, 36))
    paste_scaled(banner, turnip, (48, 48), (130, 36))
    paste_scaled(banner, player, (48, 48), (180, 30))

    banner.save(HERE / "banner.png")


if __name__ == "__main__":
    write_silent_wav(HERE / "audio.wav")
    with zipfile.ZipFile(SPROUT_ZIP) as zf:
        make_icon(zf)
        make_banner(zf)
    print("Wrote icon.png, banner.png, audio.wav from the Sprout Lands pack")
