# Elimina SOLO los assets intermedios: Firm_HorneritoV1\work\assets
# No toca: 01_Arte, work\reference ni hornerito\assets\emojis\generated.
# Usar solo cuando la verificacion este completa (assets finales + build ok).
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)   # scripts/assets -> raiz
$target = Join-Path $root 'work\assets'

if (Test-Path $target) {
    Remove-Item -Recurse -Force $target
    Write-Host "Eliminados los assets intermedios: $target"
} else {
    Write-Host "No hay carpeta que limpiar: $target"
}
