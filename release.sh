#!/bin/bash

RELEASES_DIR=./releases/powerscreen
rm -rf $RELEASES_DIR
mkdir -p $RELEASES_DIR

ASSET_NAME="powerscreen-zbolt"
PROJECT_OWNER="borferkic"
PROJECT_NAME="K1C-CFS-POWER-SCREEN"

"$CROSS_COMPILE"strip ./build/bin/powerscreen
"$CROSS_COMPILE"strip ./build/bin/kd_graphic_mode
cp ./build/bin/powerscreen $RELEASES_DIR/powerscreen
cp -r ./k1/k1_mods $RELEASES_DIR
cp -r ./k1/scripts $RELEASES_DIR
cp -r ./themes $RELEASES_DIR
cp ./installer.sh $RELEASES_DIR
cp ./update.sh $RELEASES_DIR
if [ -f ./custom_upgrade.sh ]; then
    cp ./custom_upgrade.sh $RELEASES_DIR
fi
cp reinstall-creality.sh $RELEASES_DIR
cp -r ./debian $RELEASES_DIR
cp ./build/bin/kd_graphic_mode $RELEASES_DIR/debian
cp ./k1/moonraker/powerscreen-update.conf $RELEASES_DIR


echo "{\"version\": \"$POWERSCREEN_VERSION\", \"asset_name\": \"$ASSET_NAME.tar.gz\"}" > $RELEASES_DIR/.version
echo "{\"project_name\": \"$PROJECT_NAME\", \"project_owner\": \"$PROJECT_OWNER\", \"version\": \"$POWERSCREEN_VERSION\", \"asset_name\": \"$ASSET_NAME.zip\"}" > $RELEASES_DIR/release_info.json
tar czf $ASSET_NAME.tar.gz -C releases .
python3 - "$RELEASES_DIR" "$ASSET_NAME.zip" <<'PY'
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile
import sys

source = Path(sys.argv[1])
archive = Path(sys.argv[2])
with ZipFile(archive, "w", ZIP_DEFLATED) as package:
    for item in source.rglob("*"):
        package.write(item, item.relative_to(source))
PY
