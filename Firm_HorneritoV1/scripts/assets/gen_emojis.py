#!/usr/bin/env python3
"""Genera los assets de emoji de HORNERITO para el firmware.

Flujo (los originales de 01_Arte NUNCA se modifican):

    01_Arte/emojis/basicos/*.png
        -> work/assets/original      (copia intermedia)
        -> work/assets/resized       (192x192, fondo transparente)
        -> hornerito/assets/emojis/generated/*.c  (arrays LVGL, finales)

Uso:
    python scripts/assets/gen_emojis.py

Requiere: Pillow (pip install pillow) y el conversor LVGL incluido en
scripts/Image_Converter/LVGLImage.py (pip install pypng lz4).
"""
import os
import shutil
import subprocess
import sys
import unicodedata

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ARTE = os.path.abspath(os.path.join(ROOT, "..", "01_Arte", "emojis", "basicos"))
WORK = os.path.join(ROOT, "work", "assets")
OUT = os.path.join(ROOT, "hornerito", "assets", "emojis", "generated")
CONVERTER = os.path.join(ROOT, "scripts", "Image_Converter", "LVGLImage.py")

SIZE = 192  # lado maximo del emoji en pixeles

# nombre de salida LVGL <- stem del original (sin acentos, minusculas)
EMOJIS = {
    "hornerito_idle": "saludando",       # IDLE
    "hornerito_listening": "senalando",  # LISTENING
    "hornerito_thinking": "pensando",    # THINKING
    "hornerito_speaking": "sorprendido", # SPEAKING
    "hornerito_error": "asustado",       # ERROR
}


def _norm(name: str) -> str:
    return (
        unicodedata.normalize("NFKD", name)
        .encode("ascii", "ignore")
        .decode()
        .lower()
    )


def find_source(stem: str) -> str:
    if not os.path.isdir(ARTE):
        raise SystemExit("No existe la carpeta de arte original: %s" % ARTE)
    for fname in sorted(os.listdir(ARTE)):
        if _norm(os.path.splitext(fname)[0]) == stem:
            return os.path.join(ARTE, fname)
    raise SystemExit("No se encontro el original con stem '%s' en %s" % (stem, ARTE))


def main() -> None:
    original_dir = os.path.join(WORK, "original")
    resized_dir = os.path.join(WORK, "resized")
    os.makedirs(original_dir, exist_ok=True)
    os.makedirs(resized_dir, exist_ok=True)
    os.makedirs(OUT, exist_ok=True)

    for out_name, stem in EMOJIS.items():
        src = find_source(stem)
        # 1) copia intermedia del original (01_Arte queda intacto)
        shutil.copy2(src, os.path.join(original_dir, os.path.basename(src)))
        # 2) resize manteniendo la proporcion sobre lienzo transparente
        im = Image.open(src).convert("RGBA")
        im.thumbnail((SIZE, SIZE), Image.LANCZOS)
        canvas = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
        canvas.paste(im, ((SIZE - im.width) // 2, (SIZE - im.height) // 2), im)
        canvas.save(os.path.join(resized_dir, out_name + ".png"))
        print("  resized %s.png <- %s" % (out_name, os.path.basename(src)))

    # 3) PNG -> arrays C de LVGL (RGB565A8: color + alfa)
    subprocess.check_call(
        [sys.executable, CONVERTER, "--ofmt", "C", "--cf", "RGB565A8", "-o", OUT, resized_dir]
    )

    missing = [
        n for n in EMOJIS if not os.path.isfile(os.path.join(OUT, n + ".c"))
    ]
    if missing:
        raise SystemExit("Faltan archivos generados: %s" % ", ".join(missing))
    print("OK: %d emojis generados en %s" % (len(EMOJIS), OUT))


if __name__ == "__main__":
    main()
