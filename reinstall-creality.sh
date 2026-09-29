#!/bin/sh
#
# Remove PowerScreen and restore the Creality screen and services.
# Usage: reinstall-creality.sh [--yes]
#   --yes  non-interactive: keep the backup folder (used by the CFS Power Script
#          and by the installer rollback)

BACKUP_DIR=/usr/data/powerscreen-backup
# Creality binaries disabled by the installer as "<name>.disabled".
CREALITY_BINARIES="Monitor display-server web-server"

ASSUME_YES=false
for arg in "$@"; do
    case "$arg" in
        -y|--yes) ASSUME_YES=true ;;
    esac
done

echo "Stopping PowerScreen"
[ -f /etc/init.d/S99powerscreen ] && /etc/init.d/S99powerscreen stop > /dev/null 2>&1
killall -q powerscreen
rm -f /etc/init.d/S99powerscreen

echo "Restoring Creality init scripts"
for file in S50dropbear S99start_app; do
    if [ -f "$BACKUP_DIR/$file" ]; then
        cp -p "$BACKUP_DIR/$file" /etc/init.d/$file
    fi
done

echo "Restoring Creality screen and services"
for bin in $CREALITY_BINARIES; do
    if [ -f "/usr/bin/$bin.disabled" ] && [ ! -f "/usr/bin/$bin" ]; then
        mv "/usr/bin/$bin.disabled" "/usr/bin/$bin"
    fi
done

if [ "$ASSUME_YES" = false ]; then
    printf "Do you want to delete the backup folder $BACKUP_DIR? (y/n): "
    read delete
    if [ "$delete" = "y" -o "$delete" = "Y" ]; then
        echo "Deleting $BACKUP_DIR"
        rm -rf "$BACKUP_DIR"
    else
        echo "Keeping $BACKUP_DIR"
    fi
fi

sync

if [ -f /etc/init.d/S99start_app ]; then
    /etc/init.d/S99start_app start > /dev/null 2>&1
else
    echo "S99start_app was not found; starting the Creality screen only"
    [ -x /usr/bin/Monitor ] && /usr/bin/Monitor > /dev/null 2>&1 &
    [ -x /usr/bin/display-server ] && /usr/bin/display-server > /dev/null 2>&1 &
fi

echo "Creality screen and services restored."
echo "It is recommended to restart the printer now."
