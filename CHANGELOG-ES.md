# Registro de cambios

[English](CHANGELOG.md)

Todos los cambios importantes de K1C CFS POWER SCREEN se documentan en este archivo.

## [Sin publicar]

### Añadido

- Creado el repositorio `K1C-CFS-POWER-SCREEN` para el proyecto personal de K1C.
- Añadido el botón `MANUAL M600` al panel de extrusión.
- Añadido el diálogo de cambio manual de filamento usando `PromptPanel`.
- Añadidas las acciones `SDK_UNLOAD_FILAMENT`, `SDK_LOAD_FILAMENT`, `RESUME`, `CANCEL_PRINT` y cierre.
- Añadidos iconos de carga, descarga, reanudación y detención al diálogo.
- Añadido el paquete MIPS Z-Bolt para compilaciones de K1C.
- Añadidos los canales de publicación `nightly` y `stable`.
- Añadidas instrucciones de instalación desde GitHub Releases para el paquete Z-Bolt.
- Añadidos separadores verdes alrededor de los botones de navegación de la K1C.
- Añadidos los macros SDK de filamento corregidos proporcionados para este proyecto.
- Añadido el registro de PowerScreen en el Update Manager de Moonraker/Fluidd.
- Añadida la metadata ZIP requerida por el Update Manager de Moonraker.

### Cambiado

- Simplificado el diálogo M600 con un título centrado y una distribución amplia de acciones.
- Centradas las etiquetas de acción y aumentada su fuente para facilitar el uso táctil.
- Limitado el alcance a la Creality K1C y eliminadas las variantes ARM/smallscreen de la matriz de compilación.
- Convertidos a mayúsculas los textos del diálogo personalizado y los controles de extrusión.
- Reorganizados los botones del diálogo para la pantalla táctil de la K1C.
- `RESUME` ahora es verde y `STOP` rojo.
- Reemplazado el botón inferior grande `CLOSE` por una `X` compacta durante la iteración anterior del diseño del diálogo.
- Forzada la distribución de acciones M600 como una cuadrícula de dos columnas y dos filas durante la iteración anterior del diseño del diálogo.
- Actualizada la acción de la `X` para finalizar el prompt con `action:prompt_end`.
- Corregidos el tamaño y la alineación de los botones de navegación para evitar el desplazamiento vertical acumulado.
- Añadida documentación bilingüe en el README, créditos del proyecto y el enlace de apoyo por PayPal.
- Actualizados el instalador, el actualizador, la metadata de los paquetes y las referencias del repositorio a PowerScreen.

## Formato de publicación

- `nightly`: compilaciones de desarrollo desde `main`, publicadas como pre-release.
- `stable`: compilaciones creadas a partir de tags de versión.
- Cada release proporciona `powerscreen-zbolt.tar.gz` para la instalación y `powerscreen-zbolt.zip` para el Update Manager de Moonraker.
