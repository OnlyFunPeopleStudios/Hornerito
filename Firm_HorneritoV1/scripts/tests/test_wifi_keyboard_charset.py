# Verificación de la pantalla Wi-Fi de HORNERITO:
#  1) CONFIG_LV_USE_KEYBOARD=y (widget habilitado en sdkconfig y defaults).
#  2) Los mapas por defecto del teclado LVGL permiten teclear un charset
#     realista de contraseñas Wi-Fi (minúsculas, mayúsculas, números y
#     símbolos habituales), y los modos abc/ABC/1# son alcanzables.
# Se analiza la fuente de lv_keyboard.c; no ejecuta firmware.
#
# Uso: python scripts\tests\test_wifi_keyboard_charset.py
import re
import string
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LV_KEYBOARD_C = ROOT / "managed_components" / "lvgl__lvgl" / "src" / "widgets" / "keyboard" / "lv_keyboard.c"
SDKCONFIG = ROOT / "sdkconfig"
SDKCONFIG_DEFAULTS = ROOT / "sdkconfig.defaults"

# Charset exigido por el enunciado (letras/números + símbolos habituales).
# Nota: ` ~ ^ | no están en los mapas por defecto de LVGL (limitación conocida).
REQUIRED = (
    string.ascii_lowercase
    + string.ascii_uppercase
    + string.digits
    + "!@#$%&*()_-+=.,?:;<>/\\[]{}\"'"
)


def extract_maps(src: str) -> dict:
    """name -> lista de entradas (strings C ya des-escapados)."""
    maps = {}
    pattern = re.compile(
        r"static const char \* const (default_kb_map_\w+)\s*\[\]\s*=\s*\{(.*?)\};",
        re.S,
    )
    for name, body in pattern.findall(src):
        maps[name] = tokenize(body)
    return maps


def tokenize(body: str) -> list:
    """Separa las entradas del array respetando comillas (hay entradas ',')."""
    escapes = {"n": "\n", "t": "\t", "r": "\r", "\\": "\\", '"': '"', "0": "\0"}
    items = []
    i, n = 0, len(body)
    while i < n:
        c = body[i]
        if c == '"':
            j = i + 1
            buf = []
            while j < n:
                if body[j] == "\\" and j + 1 < n:
                    buf.append(escapes.get(body[j + 1], body[j + 1]))
                    j += 2
                    continue
                if body[j] == '"':
                    break
                buf.append(body[j])
                j += 1
            items.append("".join(buf))
            i = j + 1
        else:
            i += 1
    return items


class WifiKeyboardCharsetTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = LV_KEYBOARD_C.read_text(encoding="utf-8", errors="replace")
        cls.maps = extract_maps(cls.src)
        # Cuerpos en bruto (con macros: LV_KEYBOARD_CTRL_..., LV_SYMBOL_...).
        cls.raw_maps = dict(
            re.findall(
                r"static const char \* const (default_kb_map_\w+)\s*\[\]\s*=\s*\{(.*?)\};",
                cls.src,
                re.S,
            )
        )

    def test_keyboard_widget_enabled(self):
        for path in (SDKCONFIG, SDKCONFIG_DEFAULTS):
            lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
            self.assertIn(
                "CONFIG_LV_USE_KEYBOARD=y",
                lines,
                msg=f"CONFIG_LV_USE_KEYBOARD no está activo en {path.name}",
            )

    def test_maps_present(self):
        names = set(self.maps)
        for expected in ("default_kb_map_lc", "default_kb_map_uc", "default_kb_map_spec"):
            self.assertIn(expected, names)

    def test_mode_switching_reachable(self):
        # Las modas son macros: se comprueba su presencia en el mapa en bruto.
        lc = self.raw_maps["default_kb_map_lc"]
        self.assertIn("LV_KEYBOARD_CTRL_BUTTON_MODE_TEXT_UPPER", lc,
                      "falta el modo ABC en el mapa lc")
        self.assertIn("LV_KEYBOARD_CTRL_BUTTON_MODE_SPECIAL", lc,
                      "falta el modo 1# en el mapa lc")
        uc = self.raw_maps["default_kb_map_uc"]
        self.assertIn("LV_KEYBOARD_CTRL_BUTTON_MODE_TEXT_LOWER", uc,
                      "falta el modo abc en el mapa uc")
        spec = self.raw_maps["default_kb_map_spec"]
        self.assertIn("LV_KEYBOARD_CTRL_BUTTON_MODE_TEXT_LOWER", spec,
                      "falta el modo abc en el mapa spec")

    def test_required_charset_typeable(self):
        typeable = set()
        for entries in self.maps.values():
            for entry in entries:
                if len(entry) == 1 and entry.isprintable():
                    typeable.add(entry)
        missing = sorted(set(REQUIRED) - typeable)
        self.assertEqual(
            missing,
            [],
            msg=f"el teclado no permite teclear: {''.join(missing)}",
        )

    def test_backspace_and_ok_present(self):
        # Teclas de acción del mapa por defecto (macros LV_SYMBOL_*,
        # renderizadas por Montserrat 14: rango 0x20-0x7F + símbolos LVGL).
        joined = "\n".join(self.raw_maps.values())
        self.assertIn("LV_SYMBOL_BACKSPACE", joined)
        self.assertIn("LV_SYMBOL_OK", joined)


if __name__ == "__main__":
    unittest.main(verbosity=2)
