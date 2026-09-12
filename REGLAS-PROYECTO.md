# Reglas del proyecto PowerScreen para Creality K1C

Este documento contiene las reglas específicas que deben acompañar al proyecto
si se copia a otra ubicación o repositorio. Debe leerse antes de realizar
cambios.

## Reglas obligatorias

1. **El `README.md` está protegido.** No se puede modificar, añadir contenido,
   eliminar contenido ni reordenarlo hasta que el usuario lo autorice
   textualmente por escrito.

2. **Alcance exclusivo K1C.** Las decisiones de interfaz, compilación,
   instalación y validación de este proyecto se hacen para Creality K1C/K1C
   2025. No añadir soporte ni instrucciones para otros modelos sin una orden
   explícita.

3. **Iconos del firmware K1C 2025.** La carpeta `K1c2025 icons/` contiene los
   recursos extraídos para referencia del proyecto. Debe permanecer excluida
   de Git y no debe incorporarse al paquete publicado salvo autorización
   explícita.

4. **Revisar el historial antes de cambiar el proceso.** Antes de alterar la
   compilación, el empaquetado o la publicación, revisar los commits, el
   `Makefile` y `.github/workflows/build.yml` para conservar el procedimiento
   que ya funciona.

5. **Aprobaciones separadas.** La autorización para implementar un cambio no
   equivale automáticamente a autorización para compilar o publicar. Debe
   esperarse una autorización independiente antes de compilar y otra si la
   acción implica publicar o distribuir un paquete.

6. **No afirmar validación visual o física sin evidencia.** La compilación no
   demuestra que la interfaz funcione en la pantalla ni que los macros CFS y
   el extrusor se comporten correctamente. Esas comprobaciones requieren una
   K1C real, registros o una confirmación directa.

## Proceso de compilación vigente

La compilación de producción para la K1C se realiza en GitHub Actions mediante
`.github/workflows/build.yml`, sobre Ubuntu 22.04. El workflow descarga el
toolchain MIPS, aplica los parches de los submódulos, compila las dependencias,
`powerscreen` y `kd_graphic_mode`, y genera:

- `powerscreen-zbolt.tar.gz`: paquete para instalar en la impresora.
- `powerscreen-zbolt.zip`: paquete para el Update Manager de Fluidd.

El comportamiento por rama es:

- `develop`: compila y conserva el artefacto para pruebas.
- `main`: compila y publica una nightly pre-release.
- Un tag: compila y publica una release estable.

En Windows no se debe declarar una compilación MIPS exitosa si no existe un
entorno Linux con `make`, `cmake` y el toolchain MIPS. Git Bash por sí solo no
sustituye ese entorno.

Antes de subir un cambio:

```bash
git diff --check
```

Después de subirlo, comprobar la ejecución en GitHub Actions:

```bash
gh run list --repo borferkic/K1C-CFS-POWER-SCREEN --workflow build.yml --limit 1
gh run view <RUN_ID> --repo borferkic/K1C-CFS-POWER-SCREEN
```

## Operación de la K1C mediante SSH

Estas instrucciones son la guía portable del procedimiento operativo. El
`README.md` existente también contiene esta información, pero permanece
protegido y no se modifica sin autorización textual.

### Condiciones antes de instalar

No instalar ni actualizar mientras la impresora esté imprimiendo, calentando,
haciendo homing o ejecutando una calibración.

### Conexión

Conectar como `root` usando la IP de la impresora:

```bash
ssh -p 22 root@IP_DE_TU_K1C
```

### Primera instalación

Desde la sesión SSH:

```bash
cd /usr/data
git clone --recursive https://github.com/borferkic/K1C-CFS-POWER-SCREEN.git PowerScreen
cd /usr/data/PowerScreen
sh ./installer.sh
```

### Actualización desde el clon existente

```bash
cd /usr/data/PowerScreen
git pull --recurse-submodules
sh ./installer.sh
```

El instalador descarga `powerscreen-zbolt.tar.gz`, lo extrae en
`/usr/data/powerscreen`, configura los archivos auxiliares y registra el
actualizador de Moonraker/Fluidd. El script `update.sh` descarga la release,
reemplaza el paquete y reinicia `/etc/init.d/S99powerscreen`.

### Validación después de actualizar

```bash
test -x /usr/data/powerscreen/powerscreen
test -x /etc/init.d/S99powerscreen
/etc/init.d/S99powerscreen restart
tail -n 40 /usr/data/printer_data/logs/powerscreen.log
```

Comprobar además que el proceso permanezca activo, que el log no muestre un
cierre inmediato, que Moonraker reporte `klippy_connected=true` y que la
interfaz funcione en la pantalla. La validación táctil y el comportamiento
físico del flujo CFS se realizan directamente en la K1C.

### Respaldos y rollback

La instalación crea respaldos en `/usr/data/powerscreen-backup`. No borrar
estos respaldos ni ejecutar el script de restauración sin autorización
explícita y sin confirmar primero el alcance del rollback.

## Requisito visual vigente

La ventana numérica que aparece al pulsar la temperatura desde Home debe tener
un marco verde de estilo Creality, sin alterar la entrada numérica ni sus
eventos.
