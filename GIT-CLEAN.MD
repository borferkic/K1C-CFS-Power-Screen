# Limpieza del repositorio Git de PowerScreen

**Fecha:** 2026-09-12
**Proyecto:** PowerScreen para Creality K1C
**Repositorio:** `borferkic/K1C-CFS-POWER-SCREEN`

## Objetivo

Mantener el repositorio público centrado en el código, los recursos necesarios
para compilar y ejecutar PowerScreen, los instaladores, el flujo de GitHub
Actions y las capturas que utiliza el README.

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
- `screenshots/`, porque las ocho imágenes son referenciadas por `README.md`.
- `README.md`, `LICENSE` y los recursos fuente de los iconos Z-Bolt.

## Temporales y archivos históricos

Los paquetes generados, capturas RAW, iconos extraídos, backups y registros de
trabajo no se publican desde este repositorio. Se conservan en el espacio local
organizado por la migración y están excluidos mediante las reglas aplicables.

## Validación de esta limpieza

- El repositorio remoto consultado es público y usa `main` como rama principal.
- Las ocho rutas de captura del README existen en `screenshots/`.
- No quedan enlaces del README hacia los documentos privados retirados.
- Los submódulos conservan sus commits registrados.
- Se ejecutó `git diff --check` sin errores de formato.
- No se ejecutó una compilación ni se modificaron instalaciones de la K1C.

## Reversión

Para revertir esta limpieza después de publicarla, usar un commit de reversión
con `git revert` y devolver los documentos desde
`00-Documentacion-Privada/PowerScreen/` al árbol del proyecto. No usar
`git reset --hard` ni reescribir la historia remota.
