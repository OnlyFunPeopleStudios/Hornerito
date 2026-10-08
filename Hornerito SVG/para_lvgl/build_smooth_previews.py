"""Build reviewable animated previews from the prepared transparent frames.

This only creates GIF previews and updates animation metadata. It does not
modify firmware or the source PNG frames.
"""

from __future__ import annotations

import json
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parent
MANIFEST = ROOT / "animation.json"
TARGET = (480, 480)
FPS = 12
TRANSITION_MS = 180


def fit_rgba(path: Path) -> Image.Image:
    image = Image.open(path).convert("RGBA")
    image.thumbnail(TARGET, Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", TARGET, (0, 0, 0, 0))
    canvas.alpha_composite(image, ((TARGET[0] - image.width) // 2,
                                   (TARGET[1] - image.height) // 2))
    return canvas


def composite_white(image: Image.Image) -> Image.Image:
    background = Image.new("RGBA", image.size, (255, 255, 255, 255))
    return Image.alpha_composite(background, image).convert("RGB")


def interpolate(a: Image.Image, b: Image.Image, amount: float) -> Image.Image:
    return Image.blend(a, b, amount)


def make_gif(name: str, config: dict) -> None:
    frames = [fit_rgba(ROOT / filename) for filename in config["frames"]]
    durations = config.get("duration_ms", [180] * len(frames))
    timeline: list[tuple[Image.Image, int]] = []
    for index, frame in enumerate(frames):
        timeline.append((frame, durations[index]))
        next_frame = frames[(index + 1) % len(frames)]
        if index < len(frames) - 1:  # keep the loop restart clean
            steps = max(1, round(TRANSITION_MS * FPS / 1000))
            for step in range(1, steps):
                timeline.append((interpolate(frame, next_frame, step / steps),
                                 round(1000 / FPS)))

    gif_frames = [composite_white(frame) for frame, _ in timeline]
    gif_durations = [duration for _, duration in timeline]
    output = ROOT / f"{name}_suave.gif"
    gif_frames[0].save(output, save_all=True, append_images=gif_frames[1:],
                       duration=gif_durations, loop=0, optimize=True)


def main() -> None:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    animations = manifest["animations"]
    # Use longer holds and smoother transitions for expressive gestures.
    settings = {
        "blink": {"duration_ms": [120, 100, 850]},
        "saludo_sutil": {"duration_ms": [260, 240, 240, 650]},
        "pensando": {"duration_ms": [300, 400, 1200, 650]},
        "sueno": {"duration_ms": [300, 450, 1500, 700]},
    }
    for name, values in settings.items():
        if name in animations:
            animations[name].update(values)
            make_gif(name, animations[name])
    manifest["preview_note"] = (
        "Los GIF *_suave.gif son vistas previas con fundidos entre PNG. "
        "Para LVGL se conservan PNG RGBA y la secuencia/tiempos del manifiesto."
    )
    MANIFEST.write_text(json.dumps(manifest, ensure_ascii=False, indent=2),
                        encoding="utf-8")


if __name__ == "__main__":
    main()
