[English](#english) | [Español](#espanol)

---

<a name="english"></a>

# GuppyScreen MOD for Creality K1C

This is a personal modification of GuppyScreen focused exclusively on the Creality K1C. It keeps the touch interface base for Klipper/Moonraker and adds features specific to the CFS workflow and manual filament changes.

## Scope

This repository is intended only for:

- Creality K1C.
- The original K1C screen.
- MIPS builds compatible with K1C hardware.
- Custom CFS macros and scripts.

There are no builds, instructions, or support for Android, Raspberry Pi, `GuppyDroid`, or other printer models.

## Installation from GitHub Releases

Download one of the K1C packages from the [Releases page](https://github.com/borferkic/K1C-CFS-Guppyscreen-MOD/releases) after signing in to GitHub:

- `guppyscreen.tar.gz` — Material theme.
- `guppyscreen-zbolt.tar.gz` — Z-Bolt theme.

Because this repository is private, the release package must be downloaded through an authenticated GitHub session. Copy the selected package to the K1C:

```powershell
scp .\guppyscreen.tar.gz root@IP_DE_TU_K1C:/tmp/
```

Replace the filename with `guppyscreen-zbolt.tar.gz` for the Z-Bolt theme. Then connect by SSH and replace the installed version while preserving a backup:

```sh
/etc/init.d/S99guppyscreen stop
cp -a /usr/data/guppyscreen /usr/data/guppyscreen.backup
tar xzf /tmp/guppyscreen.tar.gz -C /usr/data/
sync
/etc/init.d/S99guppyscreen restart
```

The repository provides two release channels for the K1C. `nightly` is built automatically from `main` for development testing. `stable` is built when a version tag is created. Each channel includes both the Material and Z-Bolt packages.

## Inherited features

- Console and macro shell.
- Bed Mesh.
- Input Shaper.
- Print status.
- Spoolman integration.
- Extrusion and retraction.
- Temperature control.
- Fan, LED, and movement control.
- Fine tuning for speed, flow, Z-offset, and Pressure Advance.
- Velocity and acceleration limits.
- File browser.
- TMC metrics.

## Additional features in this modification

- `MANUAL M600` button in the extrusion panel.
- Manual filament change dialog adapted to the K1C screen.
- `SDK_UNLOAD_FILAMENT` and `SDK_LOAD_FILAMENT` actions.
- Green `RESUME` action.
- Red `STOP` action using `CANCEL_PRINT`.
- `CLOSE` action to close the dialog.
- Icons for load, unload, resume, and stop actions.
- Button layout reorganized for comfortable touchscreen use on the K1C.

## Development and build

Initialize the submodules when cloning the repository:

```bash
git clone --recursive https://github.com/borferkic/K1C-CFS-Guppyscreen-MOD.git
cd K1C-CFS-Guppyscreen-MOD
```

Building for the K1C requires the MIPS toolchain described in [DEVELOPMENT.md](DEVELOPMENT.md). The main configuration uses the `mipsel-buildroot-linux-musl-` compiler and generates:

```text
build/bin/guppyscreen
```

Interface and CFS workflow testing must be performed on a real K1C. A successful build does not replace validation of the dialog, macros, or the extruder's physical behavior.

## Pending work

Pending work is tracked in [PENDIENTES.md](PENDIENTES.md).

## Support the project

If this project is useful to you, you can support its development through [PayPal](https://paypal.me/borissdk).

## Credits and thanks

This project is based on the original work from [ballaswag/guppyscreen](https://github.com/ballaswag/guppyscreen). Many thanks to its creator for developing and publishing the GuppyScreen foundation.

The projects and resources used by the original foundation are also acknowledged:

- [LVGL](https://github.com/lvgl/lvgl)
- [Material Design Icons](https://pictogrammers.com/library/mdi/)
- [Z-Bolt Icons](https://github.com/Z-Bolt/OctoScreen)
- [Moonraker](https://github.com/Arksine/moonraker)
- [KlipperScreen](https://github.com/KlipperScreen/KlipperScreen)
- [Fluidd](https://github.com/fluidd-core/fluidd)
- [Klippain Shake&Tune](https://github.com/Frix-x/klippain-shaketune)

## License

See [LICENSE](LICENSE) for the terms applicable to the original foundation and this modification.

---

<a name="espanol"></a>

# GuppyScreen MOD para Creality K1C

Esta es una modificación personal de GuppyScreen orientada exclusivamente a la Creality K1C. Mantiene la base de la interfaz táctil para Klipper/Moonraker y añade características específicas para el flujo CFS y el cambio manual de filamento.

## Alcance

Este repositorio está destinado únicamente a:

- Creality K1C.
- Pantalla original de la K1C.
- Compilación MIPS compatible con el hardware de la K1C.
- Macros y scripts personalizados para CFS.

No se incluyen builds, instrucciones ni soporte para Android, Raspberry Pi, `GuppyDroid` u otros modelos de impresora.

## Instalación desde GitHub Releases

Descarga uno de los paquetes para K1C desde la [página de Releases](https://github.com/borferkic/K1C-CFS-Guppyscreen-MOD/releases) después de iniciar sesión en GitHub:

- `guppyscreen.tar.gz` — tema Material.
- `guppyscreen-zbolt.tar.gz` — tema Z-Bolt.

Como este repositorio es privado, el paquete debe descargarse mediante una sesión autenticada de GitHub. Copia el paquete seleccionado a la K1C:

```powershell
scp .\guppyscreen.tar.gz root@IP_DE_TU_K1C:/tmp/
```

Cambia el nombre por `guppyscreen-zbolt.tar.gz` para instalar el tema Z-Bolt. Después conéctate por SSH y reemplaza la versión instalada conservando una copia de seguridad:

```sh
/etc/init.d/S99guppyscreen stop
cp -a /usr/data/guppyscreen /usr/data/guppyscreen.backup
tar xzf /tmp/guppyscreen.tar.gz -C /usr/data/
sync
/etc/init.d/S99guppyscreen restart
```

El repositorio ofrece dos canales de publicación para la K1C. `nightly` se compila automáticamente desde `main` para pruebas de desarrollo. `stable` se compila al crear un tag de versión. Cada canal incluye los paquetes Material y Z-Bolt.

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

## Apoyo al proyecto

Si este proyecto te resulta útil, puedes apoyar su desarrollo mediante [PayPal](https://paypal.me/borissdk).

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
