#!/bin/bash

RELEASES_DIR=./releases/powerscreen
rm -rf $RELEASES_DIR
mkdir -p $RELEASES_DIR

ASSET_NAME="powerscreen-zbolt"

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


echo "{\"version\": \"$POWERSCREEN_VERSION\", \"asset_name\": \"$ASSET_NAME.tar.gz\"}" > $RELEASES_DIR/.version
tar czf $ASSET_NAME.tar.gz -C releases .
