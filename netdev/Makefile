obj-m += netdev_echo.o

all: module dt
	echo Builded Device Tree Overlay and kernel module

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
dt: netdev_overlay.dts
	dtc -@ -I dts -O dtb -o netdev_overlay.dtbo netdev_overlay.dts
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf netdev_overlay.dtbo
