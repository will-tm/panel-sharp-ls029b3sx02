obj-m += panel-sharp-ls029b3sx02.o

BUILD_DIR ?= /lib/modules/$(shell uname -r)/build
DRIVER_PATH ?= /lib/modules/$(shell uname -r)/kernel/drivers/gpu/drm/panel/panel-sharp-ls029b3sx02.ko
OVERLAYS_DIR ?= /boot/overlays/

all:
	make -C $(BUILD_DIR) M=$(PWD) modules
	dtc -@ -I dts -O dtb -o vc4-kms-dsi-ls029b3sx02.dtbo vc4-kms-dsi-ls029b3sx02-overlay.dts

clean:
	rm *.dtbo
	make -C $(BUILD_DIR) M=$(PWD) clean

install:
	sudo cp -rf vc4-kms-dsi-ls029b3sx02.dtbo $(OVERLAYS_DIR)
	sudo cp -rf panel-sharp-ls029b3sx02.ko $(DRIVER_PATH)
	sudo depmod -A
