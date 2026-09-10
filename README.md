# GuppyScreen MOD para Creality K1C

Esta es una modificación personal de GuppyScreen orientada exclusivamente a la Creality K1C. Mantiene la base de la interfaz táctil para Klipper/Moonraker y añade características específicas para el flujo CFS y el cambio manual de filamento.

## Alcance

Este repositorio está destinado únicamente a:

- Creality K1C.
- Pantalla original de la K1C.
- Compilación MIPS compatible con el hardware de la K1C.
- Macros y scripts personalizados para CFS.

No se incluyen builds, instrucciones ni soporte para Android, Raspberry Pi, `GuppyDroid` u otros modelos de impresora.

## Características heredadas

- Consola y shell de macros.
- Bed Mesh.
- Input Shaper.
- Estado de impresión.
- Integración con Spoolman.
- Extrusión y retracción.
- Control de temperaturas.
- Control de ventiladores, LED y movimiento.
- Ajuste fino de velocidad, flujo, Z-offset y Pressure Advance.
- Límites de velocidad y aceleración.
- Explorador de archivos.
- Métricas TMC.

## Características adicionales de esta modificación

- Botón `MANUAL M600` en el panel de extrusión.
- Diálogo de cambio manual de filamento adaptado a la pantalla de la K1C.
- Acciones `SDK_UNLOAD_FILAMENT` y `SDK_LOAD_FILAMENT`.
- Acción `RESUME` en color verde.
- Acción `STOP` en color rojo mediante `CANCEL_PRINT`.
- Acción `CLOSE` para cerrar el diálogo.
- Iconos para las acciones de carga, descarga, reanudación y detención.
- Botones reorganizados para facilitar el uso táctil en la K1C.

## Desarrollo y compilación

Los submódulos deben inicializarse al clonar el repositorio:

```bash
git clone --recursive https://github.com/borferkic/K1C-CFS-Guppyscreen-MOD.git
cd K1C-CFS-Guppyscreen-MOD
```

La compilación para la K1C requiere el toolchain MIPS indicado en [DEVELOPMENT.md](DEVELOPMENT.md). La configuración principal usa el compilador `mipsel-buildroot-linux-musl-` y genera:

```text
build/bin/guppyscreen
```

Las pruebas de interfaz y del flujo CFS deben realizarse en una K1C real. Una compilación correcta no sustituye la validación del diálogo, las macros ni el comportamiento físico del extrusor.

## Pendientes

El trabajo pendiente se mantiene en [PENDIENTES.md](PENDIENTES.md).

## Créditos y agradecimientos

Este proyecto se basa en el trabajo original de [ballaswag/guppyscreen](https://github.com/ballaswag/guppyscreen). Muchas gracias a su creador por desarrollar y publicar la base de GuppyScreen.

También se reconocen los proyectos y recursos utilizados por la base original:

- [LVGL](https://github.com/lvgl/lvgl)
- [Material Design Icons](https://pictogrammers.com/library/mdi/)
- [Z-Bolt Icons](https://github.com/Z-Bolt/OctoScreen)
- [Moonraker](https://github.com/Arksine/moonraker)
- [KlipperScreen](https://github.com/KlipperScreen/KlipperScreen)
- [Fluidd](https://github.com/fluidd-core/fluidd)
- [Klippain Shake&Tune](https://github.com/Frix-x/klippain-shaketune)

## Licencia

Consulta [LICENSE](LICENSE) para conocer los términos aplicables a la base original y a esta modificación.
