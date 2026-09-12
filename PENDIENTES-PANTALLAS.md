# Pendientes de optimización de pantallas

Checklist de seguimiento de PowerScreen para la Creality K1C.

Estados: `[x]` implementado en código, `[~]` requiere verificación en la impresora, `[ ]` pendiente.

## Home

- [x] Barra superior completa.
- [x] Título `POWER SCREEN K1C`.
- [x] Mostrar la hora en la esquina superior derecha.
- [x] Navegación: Home, Calibrations, Console, Settings.
- [x] Eliminar Macros de la navegación.
- [x] Reubicar temperaturas y gráfico.
- [x] Quitar el fondo del gráfico.
- [x] Botón Print ancho, verde Creality, con icono y texto blancos.
- [x] Fans y LED alineados sin fondo.
- [~] Confirmar que no existan montajes ni recortes.

## Selector de archivos

- [x] Archivos presentados como pastillas.
- [x] Nombre limitado a una línea.
- [x] Eliminar la entrada `..`.
- [x] Miniatura con marco y fondo gris oscuro.
- [x] Miniatura centrada junto al nombre.
- [x] Evitar repetición de miniaturas.
- [x] Panel derecho con Print Time y Filament Weight.
- [x] Valores alineados a la derecha.
- [x] Títulos verdes y en negrita.
- [x] Barra superior con distribución `4-70-6-80-6-70-4`.
- [x] Botones de la barra en verde Creality.
- [x] Toda la pastilla seleccionable desde cualquier punto.
- [~] Confirmar en la K1C que las miniaturas aparecen y la selección táctil funciona.

## Settings

- [x] Opciones cuadradas con fondo de botón.
- [x] Iconos verdes y texto blanco.
- [x] Iconos deshabilitados en rojo.
- [x] Título `SETTINGS`.
- [x] Separación vertical de 64 px.
- [~] Confirmar centrado perfecto en pantalla.

## Calibrations

- [x] Mismo formato de botones que Settings.
- [x] Separación vertical de 64 px.
- [x] Título `CALIBRATIONS`.
- [~] Confirmar centrado perfecto.

## Console

- [x] Console como tercera opción del menú.
- [x] Título `CONSOLE`.
- [ ] Revisar si requiere optimización visual adicional.

## Filament

- [x] Categoría renombrada a `Filament`.
- [x] Icono del rodillo de filamento.
- [x] Iconos Z-Bolt para Load, Unload, Extrude y Retract.
- [~] Revisar tamaños, colores y distribución de los botones.

## System

- [x] Información de PowerScreen y versión.
- [x] Texto `Developed by Boris SdK - 2026`.
- [x] Display Sleep en `Never` por defecto.
- [x] Cambiar `Spoolman` por `CFS`.
- [~] Confirmar visualmente en la impresora.

## Tema general

- [x] Tema predeterminado verde Creality.
- [x] Botones principales con verde Creality.
- [x] Texto e iconos blancos sobre botones verdes.
- [x] Iconos deshabilitados en rojo.
- [~] Revisar uniformidad entre todas las pantallas.

## Actualizaciones

- [x] Versiones nightly con fecha y hora.
- [x] Publicación mediante GitHub Actions.
- [x] Paquete compatible con actualización manual.
- [~] Confirmar que Fluidd detecta la última nightly.
- [~] Confirmar el funcionamiento del botón Update desde Fluidd.
