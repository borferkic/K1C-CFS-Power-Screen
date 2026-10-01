#!/bin/sh
#
# Usage:
#   update.sh                       update using the configured channel
#   update.sh --check               check only (UPDATE_AVAILABLE/UP_TO_DATE)
#   update.sh --set-channel <c>     align the Moonraker Update Manager (nightly|stable)
#
# Channel: "update_channel" in powerscreenconfig.json. nightly = latest
# release (pre-releases included); stable = latest official release.

POWERSCREEN_DIR=$(dirname "$0")
VERSION_FILE=$POWERSCREEN_DIR/.version
CONFIG_FILE=$POWERSCREEN_DIR/powerscreenconfig.json
CUSTOM_UPGRADE_SCRIPT=$POWERSCREEN_DIR/custom_upgrade.sh
MOONRAKER_UPDATE_CONF=/usr/data/printer_data/config/powerscreen-update.conf
POWERSCREEN_REPOSITORY="borferkic/K1C-CFS-POWER-SCREEN"
ASSET_NAME="powerscreen-zbolt.tar.gz"
CHECK_ONLY=false
STATUS_FILE=/tmp/powerscreen-update.status
DONE_FILE=/tmp/powerscreen-update.done

# Phases read by the update screen: CHECKING, DOWNLOADING:<ver>,
# EXTRACTING:<ver>, RESTARTING:<ver>, UP_TO_DATE:<ver>, ERROR:<reason>
set_status() {
    [ "$CHECK_ONLY" = "true" ] || echo "$1" > "$STATUS_FILE"
}

# Moonraker uses "beta" to include pre-releases and "stable" for official ones.
set_moonraker_channel() {
    [ -f "$MOONRAKER_UPDATE_CONF" ] || return 0
    if [ "$1" = "stable" ]; then target=stable; else target=beta; fi
    if ! grep -q "^channel: $target\$" "$MOONRAKER_UPDATE_CONF"; then
        sed -i "s/^channel:.*/channel: $target/" "$MOONRAKER_UPDATE_CONF"
        echo "Moonraker update channel set to $target"
    fi
}

if [ "$1" = "--set-channel" ]; then
    set_moonraker_channel "$2"
    exit 0
fi

if [ "$1" = "--check" ]; then
    CHECK_ONLY=true
fi

CHANNEL=nightly
if [ -f "$CONFIG_FILE" ] && [ "`jq -r '.update_channel // empty' "$CONFIG_FILE"`" = "stable" ]; then
    CHANNEL=stable
fi

CURRENT_VERSION=""
if [ -f "$VERSION_FILE" ]; then
    CURRENT_VERSION=`jq -r '.version // empty' "$VERSION_FILE"`
fi

CURL=`which curl`
if grep -Fqs "ID=buildroot" /etc/os-release
then
    wget -q --no-check-certificate https://raw.githubusercontent.com/ballaswag/k1-discovery/main/bin/curl -O /tmp/curl
    chmod +x /tmp/curl
    CURL=/tmp/curl
fi

set_status "CHECKING"
$CURL -s https://api.github.com/repos/$POWERSCREEN_REPOSITORY/releases -o /tmp/powerscreen-releases.json
if [ "$CHANNEL" = "stable" ]; then
    RELEASE_FILTER='[.[] | select(.prerelease == false and .draft == false)][0]'
else
    RELEASE_FILTER='[.[] | select(.draft == false)][0]'
fi
latest_version=`jq -r "$RELEASE_FILTER | .tag_name" /tmp/powerscreen-releases.json 2>/dev/null`

if [ -z "$latest_version" ] || [ "$latest_version" = "null" ]; then
    if [ "$CHANNEL" = "stable" ] && jq -e 'type == "array"' /tmp/powerscreen-releases.json > /dev/null 2>&1; then
        echo "UP_TO_DATE:no stable release"
        set_status "ERROR:No stable release published yet"
        exit 0
    fi
    echo "UPDATE_CHECK_FAILED"
    set_status "ERROR:Could not reach GitHub"
    exit 1
fi

legacy_version=false
case "$CURRENT_VERSION" in
    nightly-*) legacy_version=true ;;
esac

# When moving from a nightly to the stable channel, offer the official release
# even if it sorts lower (v0.34.0 < v0.34.0-nightly.* for sort -V).
switching_to_stable=false
case "$CURRENT_VERSION" in
    *-nightly*) [ "$CHANNEL" = "stable" ] && switching_to_stable=true ;;
esac

if [ "$CURRENT_VERSION" = "$latest_version" ] || {
    [ "$legacy_version" = false ] &&
    [ "$switching_to_stable" = false ] &&
    [ "$(printf '%s\n' "$CURRENT_VERSION" "$latest_version" | sort -V | head -n1)" = "$latest_version" ]
}; then
    if [ "$CHECK_ONLY" = "true" ]; then
        echo "UP_TO_DATE:$latest_version"
    else
        echo "Current version $CURRENT_VERSION is up to date ($CHANNEL)."
        set_status "UP_TO_DATE:$latest_version"
    fi
    exit 0
else
    if [ "$CHECK_ONLY" = "true" ]; then
        echo "UPDATE_AVAILABLE:$latest_version"
        exit 0
    fi

    asset_url=`jq -r --arg asset "$ASSET_NAME" "$RELEASE_FILTER | .assets[] | select(.name == \\$asset) | .browser_download_url" /tmp/powerscreen-releases.json`
    if [ -z "$asset_url" ] || [ "$asset_url" = "null" ]; then
        set_status "ERROR:Release $latest_version has no package"
        exit 1
    fi
    echo "Downloading $CHANNEL version $latest_version, $asset_url"
    set_status "DOWNLOADING:$latest_version"
    rm -f /tmp/powerscreen.tar.gz
    if ! $CURL -L -f "$asset_url" -o /tmp/powerscreen.tar.gz || ! tar tzf /tmp/powerscreen.tar.gz > /dev/null 2>&1; then
        set_status "ERROR:Download failed"
        exit 1
    fi
fi

## override existing powerscreen
set_status "EXTRACTING:$latest_version"
if ! tar xf /tmp/powerscreen.tar.gz -C $POWERSCREEN_DIR/..; then
    set_status "ERROR:Could not extract package"
    exit 1
fi

if [ -f $CUSTOM_UPGRADE_SCRIPT ]; then
    echo "Running custom_upgrade.sh for release $latest_version"
    $CUSTOM_UPGRADE_SCRIPT
fi

## keep the Klipper macros and helper scripts of PowerScreen in sync with the package
KLIPPER_CFG_DIR=/usr/data/printer_data/config/PowerScreen
if [ -d "$KLIPPER_CFG_DIR" ] && [ -d "$POWERSCREEN_DIR/scripts" ]; then
    CFG_CHANGED=false
    for cfg in "$POWERSCREEN_DIR"/scripts/*.cfg; do
        [ -f "$cfg" ] || continue
        if ! cmp -s "$cfg" "$KLIPPER_CFG_DIR/$(basename "$cfg")"; then
            cp "$cfg" "$KLIPPER_CFG_DIR/"
            CFG_CHANGED=true
        fi
    done
    mkdir -p "$KLIPPER_CFG_DIR/scripts"
    cp "$POWERSCREEN_DIR"/scripts/*.py "$KLIPPER_CFG_DIR/scripts/" 2>/dev/null
    if [ "$CFG_CHANGED" = "true" ] && [ -x /etc/init.d/S55klipper_service ]; then
        echo "Macros changed, restarting Klipper"
        /etc/init.d/S55klipper_service restart &> /dev/null
    fi
fi

set_moonraker_channel "$CHANNEL"

echo "Updated PowerScreen to version $latest_version"
set_status "RESTARTING:$latest_version"
echo "$latest_version" > "$DONE_FILE"
# Give PowerScreen time to draw the Restarting step before it is stopped.
sleep 3
if grep -Fqs "ID=buildroot" /etc/os-release
then
    [ -f /etc/init.d/S99powerscreen ] && /etc/init.d/S99powerscreen stop &> /dev/null
    killall -q powerscreen
    /etc/init.d/S99powerscreen restart &> /dev/null
fi

exit 0
