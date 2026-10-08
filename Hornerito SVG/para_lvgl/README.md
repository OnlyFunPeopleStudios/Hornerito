# Hornerito — preparación de imágenes para LVGL

## Referencia visual

El PNG principal `Hornerio suave SVG.png` es la referencia aprobada del personaje. Los SVG de piezas son material auxiliar y no se consideran definitivos. La identidad, paleta y proporciones deben mantenerse en los cuadros futuros.

## Secuencias preparadas

- Parpadeo: `hornerito_normal.png`, `hornerito_parpadeo.png`, `hornerito_normal_2.png`.
- Saludo: `hornerito_saludo_1.png` a `hornerito_saludo_4.png`.
- Pensando: `hornerito_pensando_1.png` a `hornerito_pensando_4.png`.
- Sueño: `hornerito_sueno_1.png` a `hornerito_sueno_4.png`.

Los cuadros PNG son RGBA transparentes en lienzos de 480×480. Los GIF `*_suave.gif` son vistas previas con transiciones y pausas largas; no reemplazan cuadros intermedios dibujados ni son la salida de LVGL. `animation.json` contiene secuencias y duraciones sugeridas.

## Las 12 expresiones planificadas

1. Normal — preparada
2. Feliz — pendiente
3. Saludo — preparada
4. Sorprendido — pendiente
5. Atento — pendiente
6. Pensando — preparada
7. Durmiendo — preparada
8. Bostezando — pendiente
9. Asustado — pendiente
10. Concentrado — pendiente
11. Muerto de risa — pendiente
12. Desconfiado — pendiente

El manifiesto marca el avance. Antes de continuar con las ocho restantes, conviene revisar la coherencia del arte actual: los lienzos están normalizados, pero algunos cuadros originales aún varían en escala visible.

No se modificó firmware ni se integraron recursos al proyecto.
