#!/bin/sh

POWERSCREEN_DIR=$(dirname "$0")
VERSION_FILE=$POWERSCREEN_DIR/.version
CUSTOM_UPGRADE_SCRIPT=$POWERSCREEN_DIR/custom_upgrade.sh
POWERSCREEN_REPOSITORY="borferkic/K1C-CFS-POWER-SCREEN"
ASSET_NAME="powerscreen-zbolt.tar.gz"

if [ -f $VERSION_FILE ]; then
    CURRENT_VERSION=`cat $VERSION_FILE | jq -r .version`
fi

CURL=`which curl`
if grep -Fqs "ID=buildroot" /etc/os-release
then
    wget -q --no-check-certificate https://raw.githubusercontent.com/ballaswag/k1-discovery/main/bin/curl -O /tmp/curl
    chmod +x /tmp/curl
    CURL=/tmp/curl
fi

$CURL -s https://api.github.com/repos/$POWERSCREEN_REPOSITORY/releases -o /tmp/powerscreen-releases.json
latest_version=`jq -r '.[0].tag_name' /tmp/powerscreen-releases.json`

if [ "$(printf '%s\n' "$CURRENT_VERSION" "$latest_version" | sort -V | head -n1)" = "$latest_version" ]; then 
    echo "Current version $CURRENT_VERSION is up to date."
    exit 0
else
    asset_url=`jq -r --arg asset "$ASSET_NAME" '.[0].assets[] | select(.name == $asset) | .browser_download_url' /tmp/powerscreen-releases.json`
    echo "Downloading latest version $latest_version, $asset_url"
    $CURL -L "$asset_url" -o /tmp/powerscreen.tar.gz
fi

## override existing powerscreen
tar xf /tmp/powerscreen.tar.gz -C $POWERSCREEN_DIR/..

if [ -f $CUSTOM_UPGRADE_SCRIPT ]; then
    echo "Running custom_upgrade.sh for release $latest_version"
    $CUSTOM_UPGRADE_SCRIPT
fi

echo "Updated PowerScreen to version $latest_version"
if grep -Fqs "ID=buildroot" /etc/os-release
then
    [ -f /etc/init.d/S99powerscreen ] && /etc/init.d/S99powerscreen stop &> /dev/null
    killall -q powerscreen
    /etc/init.d/S99powerscreen restart &> /dev/null
fi

exit 0
