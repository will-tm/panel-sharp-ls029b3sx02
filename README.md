# Sharp LS029B3SX02 on Raspberry Pi
Raspberry Pi DRM panel driver for the Sharp LS029B3SX02 1440x1440 MIPI DSI display.

This project provides a Linux Direct Rendering Manager (DRM) driver for the LS029B3SX02 LCD, enabling seamless integration with the Raspberry Pi's VC4-KMS DSI pipeline. It includes:
* Kernel module for panel initialization and MIPI DSI communication.
* Device tree overlay for easy configuration.
* Support for backlight control and reset GPIOs.
* 60Hz operation by default.

## Get kernel source
Refer to [rpi-source](https://github.com/RPi-Distro/rpi-source)

## Build & Install
```
git clone https://github.com/will-tm/panel-sharp-ls029b3sx02.git
cd panel-sharp-ls029b3sx02
make
make install
```
Then edit `/boot/firmware/config.txt` to add:
```
lcd_ignore=1
dtoverlay=vc4-kms-dsi-ls029b3sx02
```
and reboot

LCD pins default to:
- Reset - GPIO26
- BL_Enable - GPIO19

Default pins can be changed, for example:
```
# Change Reset to GPIO5, BL_Enable to GPIO6
dtoverlay=vc4-kms-dsi-ls029b3sx02,bl_en=5,reset=6
```

# Hardware

Datasheet - [LS029B3SX02-Datasheet](https://285624.selcdn.ru/syms1/iblock/6ef/6ef55f3f3d739a28e10c4397ae2c7f76/ca5ab893ba0b2bd0ef0755d2fd1c05b8.pdf)
