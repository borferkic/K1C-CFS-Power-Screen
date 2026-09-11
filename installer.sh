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

ARCH=`uname -m`
if [ "$ARCH" != "mips" ]; then
    printf "${red}Unable to find compatible platform (found platform: $ARCH) ${white}\n"
    exit 1
fi

printf "${green}=== Installing PowerScreen === ${white}\n"

# check ld.so version
if [ ! -f /lib/ld-2.29.so ]; then
    printf "${red}ld.so is not the expected version. Make sure you're running 1.3.x.y firmware versions ${white}\n"
    exit 1
fi

echo "Checking for a working Moonraker"
MRK_KPY_OK=`curl localhost:7125/server/info 2> /dev/null | jq .result.klippy_connected`
if [ "$MRK_KPY_OK" != "true" ]; then
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

if [ "$1" = "nightly" ] || [ "$2" = "nightly" ]; then
    printf "${yellow}Installing nightly build ${white}\n"
    ASSET_URL="https://github.com/$POWERSCREEN_REPOSITORY/releases/download/nightly/$ASSET_NAME.tar.gz"
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

if [ ! -d "$BACKUP_DIR" ]; then
    printf "${green} Backing up original K1 files ${white}\n"
    mkdir -p $BACKUP_DIR

    mv /etc/init.d/S12boot_display $BACKUP_DIR
    cp /etc/init.d/S50dropbear $BACKUP_DIR
    cp /etc/init.d/S99start_app $BACKUP_DIR
fi

if [ ! -f $BACKUP_DIR/ft2font.cpython-38-mipsel-linux-gnu.so ]; then
    # backup ft2font
    mv /usr/lib/python3.8/site-packages/matplotlib/ft2font.cpython-38-mipsel-linux-gnu.so $BACKUP_DIR
fi

## dropbear early to ensure ssh is started with display-server
cp $K1_POWERSCREEN_DIR/k1_mods/S50dropbear /etc/init.d/S50dropbear

printf "${white}=== Do you want to disable all Creality services (revertable) with PowerScreen installation? ===\n"
printf "${green}  Pros: Frees up system resources on your K1 for critical services such as Klipper (Recommended)\n"
printf "${white}  Cons: Disabling all Creality services breaks Creality Cloud/Creality Slicer.\n\n"
printf "Disable all Creality Services? (y/n): "

read confirm_decreality
echo

if [ "$confirm_decreality" = "y" -o "$confirm_decreality" = "Y" ]; then
    printf "${green}Disabling Creality services ${white}\n"
    rm /etc/init.d/S99start_app
else
    # disables only display-server and Monitor
    mv /usr/bin/Monitor /usr/bin/Monitor.disable
    mv /usr/bin/display-server /usr/bin/display-server.disable
fi

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
printf "Restart Klipper now to pick up the new changes (y/n): "
read confirm
echo

if [ "$confirm" = "y" -o "$confirm" = "Y" ]; then
    echo "Restarting Klipper"
    /etc/init.d/S55klipper_service restart
else
    printf "${red}Some PowerScreen functionality won't work until Klipper is restarted. ${white}\n"
fi

killall -q Monitor
killall -q display-server
if [ "$confirm_decreality" = "y" -o "$confirm_decreality" = "Y" ]; then
    echo "Killing Creality services"
    killall -q master-server
    killall -q audio-server
    killall -q wifi-server
    killall -q app-server
    killall -q upgrade-server
    killall -q web-server
fi

printf "${green}Starting PowerScreen ${white}\n"
/etc/init.d/S99powerscreen restart &> /dev/null

sleep 1

ps auxw | grep powerscreen | grep -v sh | grep -v grep

if [ $? -eq 0 ]; then
    printf "${green} Successfully installed PowerScreen. Enjoy! ${white}\n"
else
    printf "${red} PowerScreen FAILED to install. Rolling back... ${white}\n"
    cp $BACKUP_DIR/S99start_app /etc/init.d/S99start_app
    rm /etc/init.d/S99powerscreen

    /etc/init.d/S99start_app restart &> /dev/null
    exit 1
fi
