#!/bin/sh
#
# Uso:
#   update.sh                       actualiza segun el canal configurado
#   update.sh --check               solo consulta (UPDATE_AVAILABLE/UP_TO_DATE)
#   update.sh --set-channel <c>     alinea el Update Manager de Moonraker (nightly|stable)
#
# Canal: "update_channel" en powerscreenconfig.json. nightly = release mas
# reciente (incluye pre-releases); stable = release oficial mas reciente.

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

# Fases que lee la pantalla de espera: CHECKING, DOWNLOADING:<ver>,
# EXTRACTING:<ver>, RESTARTING:<ver>, UP_TO_DATE:<ver>, ERROR:<motivo>
set_status() {
    [ "$CHECK_ONLY" = "true" ] || echo "$1" > "$STATUS_FILE"
}

# Moonraker usa "beta" para incluir pre-releases y "stable" para las oficiales.
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

# Al pasar de una nightly al canal stable se ofrece la oficial aunque su
# numero ordene por debajo (v0.34.0 < v0.34.0-nightly.* para sort -V).
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

set_moonraker_channel "$CHANNEL"

echo "Updated PowerScreen to version $latest_version"
set_status "RESTARTING:$latest_version"
echo "$latest_version" > "$DONE_FILE"
if grep -Fqs "ID=buildroot" /etc/os-release
then
    [ -f /etc/init.d/S99powerscreen ] && /etc/init.d/S99powerscreen stop &> /dev/null
    killall -q powerscreen
    /etc/init.d/S99powerscreen restart &> /dev/null
fi

exit 0
