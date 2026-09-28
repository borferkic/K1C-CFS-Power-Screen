#!/bin/sh

yellow=`echo "\033[01;33m"`
green=`echo "\033[01;32m"`
red=`echo "\033[01;31m"`
white=`echo "\033[m"`

BACKUP_DIR=/usr/data/powerscreen-backup
K1_POWERSCREEN_DIR=/usr/data/powerscreen
FT2FONT_PATH=/usr/lib/python3.8/site-packages/matplotlib/ft2font.cpython-38-mipsel-linux-gnu.so
POWERSCREEN_REPOSITORY="borferkic/K1C-CFS-POWER-SCREEN"
ASSET_NAME="powerscreen-zbolt"
# Creality screen and web binaries that PowerScreen disables.
CREALITY_BINARIES="Monitor display-server web-server"
# Older installers (Guppy Screen, CFS Power Script) may hold the original S99start_app.
LEGACY_BACKUP_DIRS="/usr/data/helper-script-backup/guppyscreen /usr/data/guppyscreen-backups /usr/data/guppyscreen.backup-k1c"

# Usage: installer.sh [nightly] [--yes]
#   nightly  install the latest nightly instead of the latest stable release
#   --yes    non-interactive: accept the Creality warning and restart Klipper
#            (used by the CFS Power Script after it shows its own warning)
CHANNEL=stable
ASSUME_YES=false
for arg in "$@"; do
    case "$arg" in
        nightly) CHANNEL=nightly ;;
        -y|--yes) ASSUME_YES=true ;;
    esac
done

show_creality_warning() {
    printf "${yellow}PowerScreen replaces the Creality touch screen.${white}\n\n"
    printf "The following will be DISABLED:\n"
    printf "  - Creality screen (Monitor, display-server)\n"
    printf "  - Creality services: Creality Cloud, Creality Print LAN connection\n"
    printf "    and OTA firmware updates\n\n"
    printf "Everything is backed up and restored if PowerScreen is removed.\n\n"
}

# Copy a file into the backup folder once; never overwrite an existing backup.
backup_file() {
    [ -f "$1" ] || return 0
    [ -f "$BACKUP_DIR/$(basename "$1")" ] && return 0
    cp -p "$1" "$BACKUP_DIR/"
}

ARCH=`uname -m`
if [ "$ARCH" != "mips" ]; then
    printf "${red}Unable to find compatible platform (found platform: $ARCH) ${white}\n"
    exit 1
fi

printf "${green}=== Installing PowerScreen === ${white}\n"

show_creality_warning
if [ "$ASSUME_YES" = true ]; then
    echo "Continuing (--yes)."
else
    printf "Do you want to continue? (y/n): "
    read confirm_install
    echo
    if [ "$confirm_install" != "y" -a "$confirm_install" != "Y" ]; then
        echo "Installation canceled. Nothing was changed."
        exit 1
    fi
fi

# check ld.so version
if [ ! -f /lib/ld-2.29.so ]; then
    printf "${red}ld.so is not the expected version. Make sure you're running 1.3.x.y firmware versions ${white}\n"
    exit 1
fi

echo "Checking for a working Moonraker"
MRK_KPY_OK=`curl localhost:7125/server/info 2> /dev/null | jq .result.klippy_connected`
if [ "$MRK_KPY_OK" != "true" ]; then
    if [ "$ASSUME_YES" = true ]; then
        printf "${red}Moonraker is not properly setup at port 7125. Please fix Moonraker and try again. ${white}\n"
        exit 1
    fi
    printf "${yellow}Moonraker is not properly setup at port 7125. Continue anyways? (y/n) ${white}\n"
    read confirm
    echo

    if [ "$confirm" = "y" -o "$confirm" = "Y" ]; then
	    echo "Continuing to install PowerScreen"
    else
        echo "Please fix Moonraker and restart this script."
        exit 1
    fi
fi

KLIPPER_PATH=`curl localhost:7125/printer/info 2> /dev/null | jq -r .result.klipper_path`
if [ -z "$KLIPPER_PATH" -o x"$KLIPPER_PATH" == x"null" ]; then
    KLIPPER_PATH=/usr/share/klipper
    printf "${green} Falling back to klipper path: $KLIPPER_PATH ${white}\n"
fi

printf "${green} Found klipper path: $KLIPPER_PATH ${white}\n"

KLIPPY_EXTRA_DIR=$KLIPPER_PATH/klippy/extras
GCODE_SHELL_CMD=$KLIPPY_EXTRA_DIR/gcode_shell_command.py
SHAPER_CONFIG=$KLIPPY_EXTRA_DIR/calibrate_shaper_config.py

K1_CONFIG_FILE=`curl localhost:7125/printer/info 2> /dev/null | jq -r .result.config_file`
if [ -z "$K1_CONFIG_FILE" -o x"$K1_CONFIG_FILE" == x"null" ]; then    
    K1_CONFIG_DIR=/usr/data/printer_data/config
    printf "${green} Falling back to config dir: $K1_CONFIG_DIR ${white}\n"
else
    K1_CONFIG_DIR=$(dirname "$K1_CONFIG_FILE")
    printf "${green} Found config dir: $K1_CONFIG_DIR ${white}\n"
fi

POWERSCREEN_UPDATE_CONFIG=$K1_POWERSCREEN_DIR/powerscreen-update.conf

# kill pip cache to free up overlayfs
rm -rf /root/.cache

## bootstrap for ssl support
wget -q --no-check-certificate https://raw.githubusercontent.com/ballaswag/k1-discovery/main/bin/curl -O /tmp/curl
chmod +x /tmp/curl

ASSET_URL="https://github.com/$POWERSCREEN_REPOSITORY/releases/latest/download/$ASSET_NAME.tar.gz"

if [ "$CHANNEL" = "nightly" ]; then
    # Nightlies are pre-releases named v<version>-nightly.<date>; pick the newest one.
    printf "${yellow}Installing nightly build ${white}\n"
    ASSET_URL=`/tmp/curl -s https://api.github.com/repos/$POWERSCREEN_REPOSITORY/releases | \
        jq -r --arg asset "$ASSET_NAME.tar.gz" '[.[] | select(.prerelease and (.tag_name | contains("-nightly")))][0].assets[] | select(.name == $asset) | .browser_download_url'`
    if [ -z "$ASSET_URL" ] || [ "$ASSET_URL" = "null" ]; then
        printf "${red}Could not find a nightly release. ${white}\n"
        exit 1
    fi
fi

printf "${green} Downloading asset: $ASSET_NAME.tar.gz ${white}\n"

# download/extract latest powerscreen
/tmp/curl -s -L $ASSET_URL -o /tmp/powerscreen.tar.gz
tar xf /tmp/powerscreen.tar.gz -C /usr/data/

if [ ! -f "$K1_POWERSCREEN_DIR/powerscreen" ]; then
    printf "${red}Did not find powerscreen in $K1_POWERSCREEN_DIR. PowerScreen must be extracted in $K1_POWERSCREEN_DIR ${white}\n"
    exit 1
fi

#### let's see if powerscreen starts before doing anything more
printf "${green} Test starting PowerScreen ${white}\n"
[ -f /etc/init.d/S99powerscreen ] && /etc/init.d/S99powerscreen stop &> /dev/null
killall -q powerscreen
$K1_POWERSCREEN_DIR/powerscreen &> /dev/null &

## allow powerscreen to live a little
sleep 1

ps auxw | grep powerscreen | grep -v sh | grep -v grep

if [ $? -eq 0 ]; then
    printf "${green} PowerScreen started sucessfully, continuing with installation ${white}\n"
    killall -q powerscreen
else
    printf "${red} PowerScreen FAILED to start, aborting ${white}\n"
    exit 1
fi

printf "${green}Setting up PowerScreen Macros ${white}\n"
if [ ! -f $GCODE_SHELL_CMD ]; then
    printf "${green}Installing gcode_shell_command.py for klippy ${white}\n"
    cp $K1_POWERSCREEN_DIR/k1_mods/gcode_shell_command.py $GCODE_SHELL_CMD
fi

mkdir -p $K1_CONFIG_DIR/PowerScreen/scripts
cp $K1_POWERSCREEN_DIR/scripts/*.cfg $K1_CONFIG_DIR/PowerScreen
cp $K1_POWERSCREEN_DIR/scripts/*.py $K1_CONFIG_DIR/PowerScreen/scripts

## register PowerScreen in Moonraker/Fluidd Update Manager
if [ -f "$POWERSCREEN_UPDATE_CONFIG" ] && [ -f "$K1_CONFIG_DIR/moonraker.conf" ]; then
    cp "$POWERSCREEN_UPDATE_CONFIG" "$K1_CONFIG_DIR/powerscreen-update.conf"
    if grep -q "include powerscreen-update.conf" "$K1_CONFIG_DIR/moonraker.conf"; then
        echo "moonraker.conf already includes PowerScreen updater"
    else
        printf "\n[include powerscreen-update.conf]\n" >> "$K1_CONFIG_DIR/moonraker.conf"
        echo "Registered PowerScreen in Moonraker Update Manager"
    fi
fi

## allow Moonraker to restart PowerScreen after an Update Manager upgrade
MOONRAKER_ASVC_FILE=/usr/data/printer_data/moonraker.asvc
if [ -f "$MOONRAKER_ASVC_FILE" ]; then
    if grep -qx "powerscreen" "$MOONRAKER_ASVC_FILE"; then
        echo "Moonraker service allowlist already includes PowerScreen"
    else
        printf "\npowerscreen\n" >> "$MOONRAKER_ASVC_FILE"
        echo "Added PowerScreen to Moonraker service allowlist"
    fi
fi

## includ powerscreen *.cfg in printer.cfg
if grep -q "include PowerScreen" $K1_CONFIG_DIR/printer.cfg ; then
    echo "printer.cfg already includes PowerScreen cfgs"
else
    printf "${green}Including powerscreen cfgs in printer.cfg ${white}\n"
    sed -i '/\[include gcode_macro\.cfg\]/a \[include PowerScreen/*\.cfg\]' $K1_CONFIG_DIR/printer.cfg
fi

## symlink usb
K1_GCODE_DIR=$(dirname "$K1_CONFIG_DIR")/gcodes
ln -sf /tmp/udisk $K1_GCODE_DIR/usb

printf "${green} Backing up original K1 files ${white}\n"
mkdir -p $BACKUP_DIR
backup_file /etc/init.d/S12boot_display
backup_file /etc/init.d/S50dropbear
backup_file /etc/init.d/S99start_app
# If Creality services were already disabled by an older installer, recover
# the original S99start_app from its backup so it can be restored later.
if [ ! -f "$BACKUP_DIR/S99start_app" ]; then
    for dir in $LEGACY_BACKUP_DIRS; do
        if [ -f "$dir/S99start_app" ]; then
            cp -p "$dir/S99start_app" "$BACKUP_DIR/"
            echo "Recovered S99start_app from $dir"
            break
        fi
    done
fi
rm -f /etc/init.d/S12boot_display

if [ ! -f $BACKUP_DIR/ft2font.cpython-38-mipsel-linux-gnu.so ]; then
    # backup ft2font
    mv /usr/lib/python3.8/site-packages/matplotlib/ft2font.cpython-38-mipsel-linux-gnu.so $BACKUP_DIR
fi

## dropbear early to ensure ssh is started with display-server
cp $K1_POWERSCREEN_DIR/k1_mods/S50dropbear /etc/init.d/S50dropbear

## disable the Creality screen and services (accepted in the warning above)
printf "${green}Disabling Creality screen and services ${white}\n"
rm -f /etc/init.d/S99start_app
for bin in $CREALITY_BINARIES; do
    # Same ".disabled" suffix as Guppy Screen and the CFS Power Script.
    if [ -f "/usr/bin/$bin.disable" ]; then
        mv "/usr/bin/$bin.disable" "/usr/bin/$bin.disabled"
    fi
    if [ -f "/usr/bin/$bin" ]; then
        mv "/usr/bin/$bin" "/usr/bin/$bin.disabled"
    fi
done

printf "${green}Setting up PowerScreen ${white}\n"
cp $K1_POWERSCREEN_DIR/k1_mods/S99powerscreen /etc/init.d/S99powerscreen

cp $K1_POWERSCREEN_DIR/k1_mods/calibrate_shaper_config.py $SHAPER_CONFIG

ln -sf $K1_POWERSCREEN_DIR/k1_mods/powerscreen_module_loader.py $KLIPPY_EXTRA_DIR/powerscreen_module_loader.py
ln -sf $K1_POWERSCREEN_DIR/k1_mods/powerscreen_config_helper.py $KLIPPY_EXTRA_DIR/powerscreen_config_helper.py
ln -sf $K1_POWERSCREEN_DIR/k1_mods/tmcstatus.py $KLIPPY_EXTRA_DIR/tmcstatus.py


if [ ! -d "/usr/lib/python3.8/site-packages/matplotlib-2.2.3-py3.8.egg-info" ]; then
    echo "Not replacing mathplotlib ft2font module. PSD graphs might not work"
else
    printf "${green}Replacing mathplotlib ft2font module for plotting PSD graphs ${white}\n"
    cp $K1_POWERSCREEN_DIR/k1_mods/ft2font.cpython-38-mipsel-linux-gnu.so $FT2FONT_PATH
fi

ln -sf $K1_POWERSCREEN_DIR/k1_mods/respawn/libeinfo.so.1 /lib/libeinfo.so.1
ln -sf $K1_POWERSCREEN_DIR/k1_mods/respawn/librc.so.1 /lib/librc.so.1


sync

if [ ! -f $K1_POWERSCREEN_DIR/powerscreen ]; then
    printf "${red}Installation failed, did not find powerscreen in $K1_POWERSCREEN_DIR. Make sure to extract the powerscreen directory in /usr/data. ${white}\n"
    exit 1
fi

## double check dropbear is the correct one
if ! diff $K1_POWERSCREEN_DIR/k1_mods/S50dropbear /etc/init.d/S50dropbear > /dev/null ; then
    printf "${red}Dropbear (SSHD) didn't install properly. ${white}\n"
    exit 1
fi

## request to reboot
if [ "$ASSUME_YES" = true ]; then
    confirm=y
else
    printf "Restart Klipper now to pick up the new changes (y/n): "
    read confirm
    echo
fi

if [ "$confirm" = "y" -o "$confirm" = "Y" ]; then
    echo "Restarting Klipper"
    /etc/init.d/S55klipper_service restart
else
    printf "${red}Some PowerScreen functionality won't work until Klipper is restarted. ${white}\n"
fi

echo "Stopping Creality screen and services"
killall -q Monitor
killall -q display-server
killall -q master-server
killall -q audio-server
killall -q wifi-server
killall -q app-server
killall -q upgrade-server
killall -q web-server

printf "${green}Starting PowerScreen ${white}\n"
/etc/init.d/S99powerscreen restart &> /dev/null

sleep 1

ps auxw | grep powerscreen | grep -v sh | grep -v grep

if [ $? -eq 0 ]; then
    printf "${green} Successfully installed PowerScreen. Enjoy! ${white}\n"
else
    printf "${red} PowerScreen FAILED to install. Rolling back... ${white}\n"
    sh $K1_POWERSCREEN_DIR/reinstall-creality.sh --yes
    exit 1
fi
