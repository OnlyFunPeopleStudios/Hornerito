# Capturador de logs de Hornerito

`hornerito_log.py` lee de forma independiente todo lo recibido por UART, lo muestra en consola y lo guarda incrementalmente en un documento Markdown. No filtra mensajes ni modifica el firmware de Hornerito, ESP-IDF, drivers, LVGL, Wi-Fi o audio.

## Requisitos

- Windows 11 y Python 3.
- `pyserial`. Para instalarlo:

  ```powershell
  python -m pip install pyserial
  ```

En el entorno inspeccionado ya estaba instalado `pyserial 3.5` con Python 3.12.10. La instalación local de herramientas está etiquetada ESP-IDF 6.1, pero `idf.py` e `idf_monitor.py` no estaban en el PATH de esta sesión. El capturador no depende de ellos: una lectura directa evita interferencias con el flujo completo. Para decodificar backtraces se puede ejecutar adicionalmente `idf.py monitor` desde un entorno ESP-IDF activado, aunque no debe usarse en paralelo sobre el mismo puerto COM.

## Uso

Desde la raíz del proyecto:

```powershell
python tools\hornerito_log.py
```

Por defecto usa `COM4` y `115200` baud. Seleccioná otro puerto o baudrate así:

```powershell
python tools\hornerito_log.py --port COM5
python tools\hornerito_log.py --port COM5 --baud 115200
```

Los archivos se guardan por defecto en `logs/`, que se crea automáticamente. Para elegir una ruta:

```powershell
python tools\hornerito_log.py --output logs\prueba.md
```

Cada línea se muestra en consola y se escribe con timestamp de PC y flush inmediato. `--no-timestamp` omite el timestamp agregado por la PC. El texto UART no se filtra; marcas de eventos importantes/reinicios se agregan como anotaciones Markdown separadas. `--reconnect` deja la sesión abierta y vuelve a intentar abrir el mismo COM cuando se desconecta.

Presioná `Ctrl+C` para cerrar el puerto, escribir el resumen final y guardar el documento. Para copiar el Markdown completo al portapapeles al terminar:

```powershell
python tools\hornerito_log.py --copy
```

## Ejemplos

```powershell
python tools\hornerito_log.py --port COM4 --baud 115200 --copy
python tools\hornerito_log.py --port COM5 --baud 921600 --output logs\stress.md --reconnect
```
