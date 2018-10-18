KDIR:=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux/usr/src/kernel

EXTRA_CFLAGS = -Wall -g
CC = i586-poky-linux-gcc
ARCH = x86
CROSS_COMPILE = i586-poky-linux-
SROOT=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux
DIRECT=$(PWD)
APP_HCSR = hcsr

obj-m += hcsr.o

all:
	make ARCH=x86 CROSS_COMPILE=i586-poky-linux- -C $(KDIR) M=$(DIRECT) modules
	i586-poky-linux-gcc -o $(APP_HCSR) main.c --sysroot=$(SROOT) -lpthread -Wall
clean:
	rm -f *.ko
	rm -f *.o
	rm -f Module.symvers
	rm -f modules.order
	rm -f *.mod.c
	rm -rf .tmp_versions
	rm -f *.mod.c
	rm -f *.mod.o
	rm -f \.*.cmd
	rm -f Module.markers
	rm -f $(APP)
