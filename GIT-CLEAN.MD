<!--
METADATOS DEL DOCUMENTO
Nombre: `GIT-CLEAN.MD`
Fecha de creación: `2026-09-12`
Descripción: Registro de limpieza y límites del repositorio público de PowerScreen.
Proyecto: `PowerScreen`
Última modificación: `2026-09-12`
-->

<!--
METADATOS DEL DOCUMENTO
Nombre: `GIT-CLEAN.MD`
Fecha de creación: `2026-09-12`
Descripción: Registro de limpieza y límites del repositorio público de PowerScreen.
Proyecto: `PowerScreen`
Última modificación: `2026-09-12`
-->

# Limpieza del repositorio Git de PowerScreen

**Fecha:** 2026-09-12
**Proyecto:** PowerScreen para Creality K1C
**Repositorio:** `borferkic/K1C-CFS-POWER-SCREEN`

## Objetivo

Mantener el repositorio público centrado en el código, los recursos necesarios
para compilar y ejecutar PowerScreen, los instaladores y el flujo de GitHub
Actions. La evidencia visual se volverá a publicar cuando represente la
interfaz actual.

La documentación de desarrollo, las reglas internas, el backlog de trabajo y
el historial de versiones se conservan fuera del árbol público en
`00-Documentacion-Privada/PowerScreen/`.

## Documentos retirados del árbol público

Los siguientes archivos se conservan localmente, pero dejan de formar parte de
la rama pública `main`:

- `DEVELOPMENT.md`: procedimiento interno de compilación y pruebas.
- `CHANGELOG.md`: historial privado de versiones en inglés.
- `CHANGELOG-ES.md`: historial privado de versiones en español.
- `REGLAS-PROYECTO.md`: reglas operativas y de validación del proyecto.
- `PENDIENTES-PANTALLAS.md`: checklist interno de optimización visual.

No se borraron las copias locales. La limpieza del árbol actual tampoco
reescribe el historial Git ni elimina tags o releases existentes.

## Elementos conservados

- `src/`, `assets/`, `k1/`, `debian/`, `themes/` y `patches/`.
- `Makefile`, `VERSION`, scripts de instalación, actualización y release.
- Workflow de `.github/workflows/build.yml`.
- Submódulos `lvgl`, `lv_drivers`, `libhv` y `spdlog`.
- `wpa_supplicant`, requerido por el objetivo `wpaclient` del build.
- `README.md`, `LICENSE` y los recursos fuente de los iconos Z-Bolt.

## Material visual retirado

- Las ocho imágenes antiguas de `screenshots/` se retiraron del árbol público
  porque ya no representan la interfaz actual.
- Las copias verificadas se conservaron en
  `90-Archivo-Local/PowerScreen/capturas-duplicadas/screenshots-viejos-powerscreen/`.
- Se podrán volver a publicar capturas nuevas cuando la interfaz esté lista.

## Temporales y archivos históricos

Los paquetes generados, capturas RAW, iconos extraídos, backups y registros de
trabajo no se publican desde este repositorio. Se conservan en el espacio local
organizado por la migración y están excluidos mediante las reglas aplicables.

## Validación de esta limpieza

- El repositorio remoto consultado es público y usa `main` como rama principal.
- La sección de capturas del README fue reemplazada por un aviso temporal y ya
  no existen imágenes antiguas publicadas en `screenshots/`.
- No quedan enlaces del README hacia los documentos privados retirados.
- El README ya no muestra capturas antiguas; la sección indica `Coming soon...
  working on it.` hasta disponer de imágenes que representen la interfaz real.
- Los submódulos conservan sus commits registrados.
- Se ejecutó `git diff --check` sin errores de formato.
- El push del commit `23be7d8` ejecutó Actions `34689227751`; los jobs `build`
  y `release` terminaron correctamente.
- Actions generó la nightly `v0.33.3-nightly.20260912.104520Z`.
- No se modificó ni instaló nada directamente en la K1C.

## Revisión de referencias del nombre anterior

- Se revisó el árbol activo de `main` y no quedan rutas, nombres ni contenido
  propio de PowerScreen con el nombre anterior de la interfaz.
- No existen issues ni pull requests públicos con comentarios pendientes que
  contengan referencias al nombre anterior.
- GitHub Discussions está deshabilitado para este repositorio, por lo que no
  hay comentarios de Discussions que limpiar.
- Los commits y tags históricos todavía pueden contener referencias al nombre
  anterior. No se reescribió el historial ni se eliminaron tags, porque eso
  rompería enlaces, referencias y releases existentes.
- Las dependencias externas no fueron modificadas.

## Reversión

Para revertir esta limpieza después de publicarla, usar un commit de reversión
con `git revert`, devolver los documentos desde
`00-Documentacion-Privada/PowerScreen/` y, si se necesitan imágenes, restaurar
las copias desde `90-Archivo-Local/PowerScreen/capturas-duplicadas/`.
No usar `git reset --hard` ni reescribir la historia remota.
