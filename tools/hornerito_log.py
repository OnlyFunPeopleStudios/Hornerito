#!/usr/bin/env python3
"""Capture the complete UART stream from Hornerito into a Markdown session log."""

from __future__ import annotations

import argparse
import datetime as dt
import os
import re
import sys
import time
from pathlib import Path

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None
    list_ports = None


EVENT_PATTERNS = re.compile(
    r"Guru Meditation|PANIC|abort\(\)|Backtrace|\brst:|Brownout|watchdog|"
    r"Load access fault|IllegalInstruction|Store access fault|"
    r"Instruction access fault|Stack protection fault",
    re.IGNORECASE,
)
RESET_PATTERN = re.compile(r"\brst:|\bboot:\s|ESP-ROM:|(?:^|\s)rst\s+reason", re.IGNORECASE)
CHUNK_SIZE = 4096


def timestamp(value: dt.datetime | None = None) -> str:
    value = value or dt.datetime.now()
    return value.strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]


def default_output_path() -> Path:
    return Path("logs") / f"Hornerito_{dt.datetime.now():%Y-%m-%d_%H-%M-%S}.md"


def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Captura y guarda todo el flujo UART de Hornerito en Markdown."
    )
    parser.add_argument("--port", default="COM4", help="Puerto serie (por defecto: COM4).")
    parser.add_argument("--baud", type=int, default=115200, help="Baudrate (por defecto: 115200).")
    parser.add_argument("--output", type=Path, help="Archivo Markdown de salida.")
    parser.add_argument("--copy", action="store_true", help="Copia el log completo al portapapeles al terminar.")
    parser.add_argument("--no-timestamp", action="store_true", help="No antepone timestamp de PC a cada línea.")
    parser.add_argument("--reconnect", action="store_true", help="Espera y reintenta abrir el puerto tras una desconexión.")
    return parser


class MarkdownLog:
    def __init__(self, path: Path, port: str, baud: int, started: dt.datetime, no_timestamp: bool):
        self.path = path
        self.started = started
        self.no_timestamp = no_timestamp
        self.lines = 0
        self.pending = bytearray()
        path.parent.mkdir(parents=True, exist_ok=True)
        self.file = path.open("w", encoding="utf-8", errors="backslashreplace", newline="")
        self.file.write(
            "# HORNERITO — LOG DE SESIÓN\n\n"
            f"- Inicio: {started:%Y-%m-%d %H:%M:%S}\n"
            f"- Puerto: {port}\n"
            f"- Baudrate: {baud}\n"
            "- Herramienta: hornerito_log.py\n\n"
            "---\n\n## LOG\n\n"
        )
        self.file.flush()

    def write_bytes(self, data: bytes) -> list[str]:
        """Accumulate arbitrary byte chunks and emit complete lines reversibly."""
        self.pending.extend(data)
        events: list[str] = []
        start = 0
        index = 0
        while index < len(self.pending):
            byte = self.pending[index]
            if byte not in (10, 13):
                index += 1
                continue
            if byte == 13 and index + 1 == len(self.pending):
                break  # CRLF may straddle UART reads.
            ending_len = 2 if byte == 13 and self.pending[index + 1] == 10 else 1
            body = bytes(self.pending[start:index])
            self._write_line(body, bytes(self.pending[index:index + ending_len]))
            decoded = body.decode("utf-8", errors="backslashreplace")
            if EVENT_PATTERNS.search(decoded):
                events.append(decoded)
            index += ending_len
            start = index
        if start:
            del self.pending[:start]
        self.file.flush()
        return events

    def _write_line(self, raw: bytes, ending: bytes) -> None:
        body = raw.decode("utf-8", errors="backslashreplace")
        self.lines += 1
        rendered = body if self.no_timestamp else f"[{timestamp()[11:]}] {body}"
        self.file.write(rendered + ("\n" if ending else ""))
        sys.stdout.write(body + ("\n" if ending else ""))

    def annotation(self, message: str) -> None:
        self.file.write(f"\n> **{timestamp()} — {message}**\n\n")
        self.file.flush()

    def finish(self, ended: dt.datetime) -> None:
        if self.pending:
            raw = bytes(self.pending)
            self._write_line(raw, b"")
            decoded = raw.decode("utf-8", errors="backslashreplace")
            if EVENT_PATTERNS.search(decoded):
                print("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n!!! HORNERITO EVENTO IMPORTANTE !!!\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
                print(f"[EVENTO] {decoded}")
                self.annotation(f"EVENTO DETECTADO: {decoded}")
            self.pending.clear()
        duration = ended - self.started
        seconds = int(duration.total_seconds())
        elapsed = f"{seconds // 3600:02d}:{seconds % 3600 // 60:02d}:{seconds % 60:02d}"
        self.file.write(
            "\n---\n\n## FIN DE SESIÓN\n\n"
            f"- Fin: {ended:%Y-%m-%d %H:%M:%S}\n"
            f"- Duración: {elapsed}\n"
            f"- Líneas capturadas: {self.lines}\n"
        )
        self.file.flush()
        os.fsync(self.file.fileno())
        self.file.close()


def open_serial(port: str, baud: int):
    return serial.Serial(port=port, baudrate=baud, timeout=0.25, write_timeout=0)


def copy_to_clipboard(path: Path) -> bool:
    try:
        import tkinter

        root = tkinter.Tk()
        root.withdraw()
        root.clipboard_clear()
        root.clipboard_append(path.read_text(encoding="utf-8"))
        root.update()
        root.destroy()
        return True
    except Exception as exc:
        print(f"[ERROR] No se pudo copiar al portapapeles: {exc}", file=sys.stderr)
        return False


def main() -> int:
    args = make_parser().parse_args()
    if serial is None:
        print("Falta pyserial. Instalalo con: python -m pip install pyserial", file=sys.stderr)
        return 2
    started = dt.datetime.now()
    output = args.output or default_output_path()
    try:
        log = MarkdownLog(output, args.port, args.baud, started, args.no_timestamp)
    except OSError as exc:
        print(f"[ERROR] No se pudo crear {output}: {exc}", file=sys.stderr)
        return 1

    device = None
    failed_to_connect = False
    try:
        try:
            device = open_serial(args.port, args.baud)
        except (serial.SerialException, OSError) as exc:
            if not args.reconnect:
                raise
            print(f"[ERROR] No se pudo abrir {args.port}. Puede estar desconectado o en uso por otro programa. Esperando reconexión... ({exc})", file=sys.stderr)
        print(f"Capturando {args.port} a {args.baud} baud. Ctrl+C para finalizar.")
        print(f"Archivo: {output}")
        while True:
            if device is None:
                try:
                    device = open_serial(args.port, args.baud)
                    print(f"[INFO] Conectado a {args.port}.")
                except (serial.SerialException, OSError) as exc:
                    print(f"[ERROR] {args.port} sigue sin estar disponible; reintento en 2 segundos. Cerrá cualquier monitor serie que lo esté usando. ({exc})", file=sys.stderr)
                    time.sleep(2)
                    continue
            try:
                data = device.read(CHUNK_SIZE)
                if data:
                    for event in log.write_bytes(data):
                        print("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n!!! HORNERITO EVENTO IMPORTANTE !!!\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
                        print(f"[EVENTO] {event}")
                        log.annotation(f"EVENTO DETECTADO: {event}")
                        if RESET_PATTERN.search(event):
                            print("========== HORNERITO REINICIO DETECTADO ==========")
                            log.annotation("HORNERITO REINICIO DETECTADO")
            except (serial.SerialException, OSError) as exc:
                device.close()
                device = None
                print(f"[ERROR] Dispositivo desconectado de {args.port}. La sesión continúa abierta / esperando reconexión... ({exc})", file=sys.stderr)
                if not args.reconnect:
                    break
    except KeyboardInterrupt:
        pass
    except (serial.SerialException, OSError) as exc:
        message = f"No se pudo abrir {args.port}. Puede estar desconectado o en uso por otro programa. Cerrá ESP-IDF Monitor, Arduino Serial Monitor u otra terminal serie e intentá de nuevo. Detalle: {exc}"
        print(f"[ERROR] {message}", file=sys.stderr)
        log.annotation(f"ERROR DE CONEXIÓN: {message}")
        failed_to_connect = True
    finally:
        if device is not None:
            device.close()
        ended = dt.datetime.now()
        log.finish(ended)

    elapsed = int((ended - started).total_seconds())
    duration = f"{elapsed // 3600:02d}:{elapsed % 3600 // 60:02d}:{elapsed % 60:02d}"
    if failed_to_connect:
        return 1
    print("\nCaptura finalizada.")
    print(f"Archivo: {output}")
    print(f"Líneas capturadas: {log.lines}")
    print(f"Duración: {duration}")
    if args.copy:
        print("Log copiado al portapapeles." if copy_to_clipboard(output) else "No se pudo copiar el log.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
