"""Tetris: valida las tablas kShapeX/kShapeY y el wall-kick de Rotate().

Lee tetris.cc (tablas de formas y offsets de kick reales del código) y
comprueba que:
  1. cada pieza/rotación tiene 4 celdas distintas dentro de la caja 4x4;
  2. con la pieza pegada a pared y/o suelo (tablero vacío) el conjunto de
     kicks de Rotate() siempre encuentra sitio para la rotación siguiente.

Ejecutar: python scripts/tests/test_tetris_shapes.py
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TETRIS = ROOT / "hornerito" / "games" / "tetris.cc"
SOURCE = TETRIS.read_text(encoding="utf-8")

COLS = 10
ROWS = 20


def _array(name):
    """Extrae kName[...] = {...}; como lista aplanada de enteros."""
    match = re.search(rf"{name}(?:\[\d+\])+\s*=\s*\{{(.*?)\}};", SOURCE, re.S)
    assert match, f"{name} no encontrada en tetris.cc"
    return [int(n) for n in re.findall(r"-?\d+", match.group(1))]


def _cell(numbers, piece, rot, cell):
    return numbers[piece * 16 + rot * 4 + cell]


def can_place(sx, sy, piece, rot, px, py):
    """Réplica de TetrisGame::CanPlace con tablero vacío."""
    for i in range(4):
        bx = px + _cell(sx, piece, rot, i)
        by = py + _cell(sy, piece, rot, i)
        if bx < 0 or bx >= COLS or by >= ROWS:
            return False
    return True


class TetrisShapesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sx = _array("kShapeX")
        cls.sy = _array("kShapeY")
        cls.kick_x = _array("kKickX")
        cls.kick_y = _array("kKickY")
        assert len(cls.sx) == 7 * 16 and len(cls.sy) == 7 * 16
        assert len(cls.kick_x) == 5 and len(cls.kick_y) == 3

    def test_four_distinct_cells_inside_4x4_box(self):
        for piece in range(7):
            for rot in range(4):
                cells = {
                    (_cell(self.sx, piece, rot, i), _cell(self.sy, piece, rot, i))
                    for i in range(4)
                }
                self.assertEqual(
                    len(cells), 4, f"pieza {piece} rot {rot}: {sorted(cells)}"
                )
                for x, y in cells:
                    self.assertTrue(
                        0 <= x <= 3 and 0 <= y <= 3,
                        f"pieza {piece} rot {rot}: {(x, y)} fuera de la caja 4x4",
                    )

    def test_kick_rotation_always_fits_at_walls_and_floor(self):
        for piece in range(7):
            for rot in range(4):
                nxt = (rot + 1) % 4
                min_x = min(_cell(self.sx, piece, rot, i) for i in range(4))
                max_x = max(_cell(self.sx, piece, rot, i) for i in range(4))
                max_y = max(_cell(self.sy, piece, rot, i) for i in range(4))
                origins = (
                    ("pared izq", -min_x, 0),
                    ("pared der", COLS - 1 - max_x, 0),
                    ("suelo", 3, ROWS - 1 - max_y),
                    ("esquina der", COLS - 1 - max_x, ROWS - 1 - max_y),
                    ("esquina izq", -min_x, ROWS - 1 - max_y),
                )
                for name, px, py in origins:
                    self.assertTrue(
                        can_place(self.sx, self.sy, piece, rot, px, py),
                        f"{name}: pieza {piece} rot {rot} no parte de posición válida",
                    )
                    fits = any(
                        can_place(self.sx, self.sy, piece, nxt, px + kx, py + ky)
                        for ky in self.kick_y
                        for kx in self.kick_x
                        if py + ky >= 0
                    )
                    self.assertTrue(
                        fits,
                        f"{name}: pieza {piece} {rot}->{nxt} no gira con los kicks",
                    )


if __name__ == "__main__":
    unittest.main()
