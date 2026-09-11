# Changelog

[Español](CHANGELOG-ES.md)

All notable changes to K1C CFS POWER SCREEN are documented in this file.

## [Unreleased]

### Added

- Created the `K1C-CFS-POWER-SCREEN` repository for the personal K1C project.
- Added the `MANUAL M600` button to the extrusion panel.
- Added the manual filament change dialog using `PromptPanel`.
- Added `SDK_UNLOAD_FILAMENT`, `SDK_LOAD_FILAMENT`, `RESUME`, `CANCEL_PRINT`, and close actions.
- Added load, unload, resume, and stop icons to the dialog.
- Added the Z-Bolt MIPS package for K1C builds.
- Added `nightly` and `stable` release channels.
- Added GitHub Release installation instructions for the Z-Bolt package.
- Added green separators around the K1C navigation buttons.
- Added the corrected SDK filament macros supplied for this project.
- Added Moonraker/Fluidd Update Manager registration for PowerScreen.
- Added ZIP release metadata required by Moonraker's Update Manager.

### Changed

- Updated nightly release versions to a Fluidd-compatible SemVer format with UTC timestamps.
- Simplified the M600 dialog to a centered title and a large action layout.
- Centered the action labels and increased their font size for easier touchscreen use.
- Limited the project scope to the Creality K1C and removed ARM/smallscreen variants from the build matrix.
- Converted the custom dialog text and extrusion controls to uppercase.
- Reorganized the dialog buttons for the K1C touchscreen.
- Made `RESUME` green and `STOP` red.
- Replaced the large `CLOSE` footer button with a compact `X` in the top-right corner during the earlier dialog layout iteration.
- Enforced the M600 action layout as a two-column, two-row grid during the earlier dialog layout iteration.
- Updated the `X` action to finish the prompt with `action:prompt_end`.
- Corrected navigation button sizing and alignment to prevent cumulative vertical drift.
- Added bilingual README documentation, project credits, and the PayPal support link.
- Updated the installer, updater, package metadata, and repository references to PowerScreen.

## Release format

- `nightly`: development builds from `main`, published as a pre-release.
- `stable`: builds created from version tags.
- Each release provides `powerscreen-zbolt.tar.gz` for installation and `powerscreen-zbolt.zip` for Moonraker's Update Manager.
