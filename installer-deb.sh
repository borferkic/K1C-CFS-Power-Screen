#!/bin/sh

yellow=`echo "\033[01;33m"`
green=`echo "\033[01;32m"`
red=`echo "\033[01;31m"`
white=`echo "\033[m"`

KLIPPER_PATH="${HOME}/klipper"
KLIPPY_EXTRA_DIR=$KLIPPER_PATH/klippy/extras
GCODE_SHELL_CMD=$KLIPPY_EXTRA_DIR/gcode_shell_command.py
SHAPER_CONFIG=$KLIPPY_EXTRA_DIR/calibrate_shaper_config.py
CONFIG_FILE="${HOME}/printer_data/config"
PRINTER_DATA_DIR="${HOME}/printer_data"
POWERSCREEN_DIR="${HOME}/powerscreen"
POWERSCREEN_REPOSITORY="borferkic/K1C-CFS-POWER-SCREEN"

has_moonraker() {
    echo "Checking for a working Moonraker"
    MRK_KPY_OK=`curl -s localhost:7125/server/info | jq .result.klippy_connected`
    if [ "$MRK_KPY_OK" != "true" ]; then
	printf "${yellow}Moonraker is not properly setup at port 7125. Please fix moonraker ${white}\n"
    fi
}

get_klipper_paths() {
    KLIPPER_PATH=`curl -s localhost:7125/printer/info | jq -r .result.klipper_path`
    if [ "$KLIPPER_PATH" = "null" ]; then
	printf "${green} Falling back to klipper path: $KLIPPER_PATH ${white}\n"
    fi

    printf "${green} Found klipper path: $KLIPPER_PATH ${white}\n"

    KLIPPY_EXTRA_DIR=$KLIPPER_PATH/klippy/extras
    GCODE_SHELL_CMD=$KLIPPY_EXTRA_DIR/gcode_shell_command.py
    SHAPER_CONFIG=$KLIPPY_EXTRA_DIR/calibrate_shaper_config.py


    CONFIG_FILE=`curl -s localhost:7125/printer/info | jq -r .result.config_file`
    if [ "$CONFIG_FILE" = "null" ]; then    
	printf "${green} Falling back to config dir: $CONFIG_DIR ${white}\n"
    fi

    CONFIG_DIR=$(dirname "$CONFIG_FILE")
    PRINTER_DATA_DIR=$(dirname "$CONFIG_DIR")    
    printf "${green} Found config dir: $CONFIG_DIR ${white}\n"
    printf "${green} Found printer_data dir: $PRINTER_DATA_DIR ${white}\n"
    
}

install_services() {
    sed -i "s|<USER>|$USER|g" ${HOME}/powerscreen/debian/powerscreen.service

    sudo cp ${HOME}/powerscreen/debian/disable_blinking_cursor.service /etc/systemd/system
    sudo cp ${HOME}/powerscreen/debian/powerscreen.service /etc/systemd/system
    sudo systemctl enable disable_blinking_cursor.service
    sudo systemctl enable powerscreen.service
    printf "${green}Configuring powerscreen services ${white}\n"

    sudo systemctl disable KlipperScreen.service

    # allow users in netdev group to modify wifi
    if grep -q "GROUP=netdev" /lib/systemd/system/wpa_supplicant.service ; then
	printf "${green}WPA Supplicant already grants access to GROUP netdev ${white}\n"
    else
	sudo sed -i 's/\/run\/wpa_supplicant/"DIR=\/run\/wpa_supplicant GROUP=netdev"/g' /lib/systemd/system/wpa_supplicant.service
    fi
}

install_powerscreen_goodies() {
    printf "${green}Setting up PowerScreen Macros ${white}\n"
    if [ ! -f $GCODE_SHELL_CMD ]; then
	printf "${green}Installing gcode_shell_command.py for klippy ${white}\n"
	cp $POWERSCREEN_DIR/k1_mods/gcode_shell_command.py $GCODE_SHELL_CMD
    fi

    cp $POWERSCREEN_DIR/k1_mods/calibrate_shaper_config.py $SHAPER_CONFIG

    mkdir -p $CONFIG_DIR/PowerScreen/scripts
    sed -i "s|<CONFIG_DIR>|$CONFIG_DIR|g; s|<KLIPPER_PATH>|$KLIPPER_PATH|g" $POWERSCREEN_DIR/debian/powerscreen_cmd.cfg
    cp $POWERSCREEN_DIR/debian/*.cfg $CONFIG_DIR/PowerScreen
    cp $POWERSCREEN_DIR/scripts/*.py $CONFIG_DIR/PowerScreen/scripts
    
    if grep -q "include PowerScreen" $CONFIG_DIR/printer.cfg ; then
	echo "printer.cfg already includes PowerScreen cfgs"
    else
	printf "${green}Including powerscreen cfgs in printer.cfg ${white}\n"
	sed -i '1s;^;\[include PowerScreen/*\.cfg\]\n;' $CONFIG_DIR/printer.cfg
    fi

    sed -i "s|<POWERSCREEN_DIR>|$POWERSCREEN_DIR|g; s|<PRINTER_DATA_DIR>|$PRINTER_DATA_DIR|g" $POWERSCREEN_DIR/debian/powerscreenconfig.json
    
    cp $POWERSCREEN_DIR/debian/powerscreenconfig.json $POWERSCREEN_DIR
    mkdir $POWERSCREEN_DIR/thumbnails
}

restart_services() {
    printf "${green}Restarting PowerScreen services ${white}\n"
    sudo service disable_blinking_cursor restart
    service powerscreen restart
}


ARCH=`uname -m`
echo "Found arch $ARCH"

if [ "$ARCH" = "aarch64" ]; then
    printf "${green}Installing PowerScreen ${white}\n"

    # Este instalador ARM requiere que el repositorio propio publique este artefacto.
    ASSET_URL="https://github.com/$POWERSCREEN_REPOSITORY/releases/latest/download/powerscreen-arm.tar.gz"
    if [ "$1" = "nightly" ]; then
        printf "${yellow}Installing nightly build ${white}\n"
        ASSET_URL="https://github.com/$POWERSCREEN_REPOSITORY/releases/download/nightly/powerscreen-arm.tar.gz"
    fi
    
    curl -s -L $ASSET_URL -o /tmp/powerscreen.tar.gz
    tar xf /tmp/powerscreen.tar.gz -C ${HOME}

    has_moonraker
    get_klipper_paths
    install_services
    install_powerscreen_goodies
    restart_services

    printf "${green}Successfully installed PowerScreen ${white}\n"
else
    printf "${red}Terminating... Your OS Platform has not been tested with PowerScreen ${white}\n"
    exit 1
fi
