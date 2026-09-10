# Changelog

All notable changes to the K1C modification are documented in this file.

Todos los cambios importantes de esta modificación para K1C se documentan en este archivo.

## [Unreleased] / [Sin publicar]

### Added / Añadido

- Created the private repository `K1C-CFS-Guppyscreen-MOD` for the personal K1C project.
- Creado el repositorio privado `K1C-CFS-Guppyscreen-MOD` para el proyecto personal de K1C.
- Added the `MANUAL M600` button to the extrusion panel.
- Añadido el botón `MANUAL M600` al panel de extrusión.
- Added the manual filament change dialog using `PromptPanel`.
- Añadido el diálogo de cambio manual de filamento usando `PromptPanel`.
- Added `SDK_UNLOAD_FILAMENT`, `SDK_LOAD_FILAMENT`, `RESUME`, `CANCEL_PRINT`, and close actions.
- Añadidas las acciones `SDK_UNLOAD_FILAMENT`, `SDK_LOAD_FILAMENT`, `RESUME`, `CANCEL_PRINT` y cierre.
- Added load, unload, resume, and stop icons to the dialog.
- Añadidos iconos de carga, descarga, reanudación y detención al diálogo.
- Added Material and Z-Bolt MIPS packages for K1C builds.
- Añadidos paquetes MIPS Material y Z-Bolt para compilaciones de K1C.
- Added `nightly` and `stable` release channels.
- Añadidos los canales de publicación `nightly` y `stable`.
- Added GitHub Release installation instructions for both themes.
- Añadidas instrucciones de instalación desde GitHub Releases para ambos temas.

### Changed / Cambiado

- Limited the project scope to the Creality K1C and removed ARM/smallscreen variants from the build matrix.
- Limitado el alcance a la Creality K1C y eliminadas las variantes ARM/smallscreen de la matriz de compilación.
- Converted the custom dialog text and extrusion controls to uppercase.
- Convertidos a mayúsculas los textos del diálogo personalizado y los controles de extrusión.
- Reorganized the dialog buttons for the K1C touchscreen.
- Reorganizados los botones del diálogo para la pantalla táctil de la K1C.
- Made `RESUME` green and `STOP` red.
- `RESUME` ahora es verde y `STOP` rojo.
- Replaced the large `CLOSE` footer button with a compact `X` in the top-right corner.
- Reemplazado el botón inferior grande `CLOSE` por una `X` compacta en la esquina superior derecha.
- Added bilingual README documentation, project credits, and the PayPal support link.
- Añadida documentación bilingüe en el README, créditos del proyecto y el enlace de apoyo por PayPal.
- Updated the K1C remotely through SSH using the Z-Bolt nightly package, preserving a device backup.
- Actualizada remotamente la K1C por SSH usando el paquete nightly Z-Bolt, conservando un respaldo del dispositivo.

### Pending / Pendiente

- Remove the `GuppyDroid` workflow and any remaining non-K1C references from the repository.
- Eliminar el workflow de `GuppyDroid` y cualquier referencia restante que no corresponda a K1C.
- Validate the final dialog and CFS workflow on the physical K1C.
- Validar el diálogo final y el flujo CFS en la K1C física.

## Release format / Formato de publicación

- `nightly`: builds from `main` for development testing.
- `nightly`: compilaciones desde `main` para pruebas de desarrollo.
- `stable`: builds created from version tags.
- `stable`: compilaciones creadas a partir de tags de versión.
- Each channel provides `guppyscreen.tar.gz` (Material) and `guppyscreen-zbolt.tar.gz` (Z-Bolt).
- Cada canal proporciona `guppyscreen.tar.gz` (Material) y `guppyscreen-zbolt.tar.gz` (Z-Bolt).
