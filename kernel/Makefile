# phmeop — módulo de kernel Linux (/dev/phmeop)
#
# Compilación en Linux de escritorio (contra el kernel en ejecución):
#   make
#
# Compilación cruzada para Android (arm64), ejemplo:
#   make ARCH=arm64 CROSS_COMPILE=aarch64-linux-android- KDIR=/ruta/al/kernel/android
#
# El kernel de destino debe estar configurado y compilado (con .config)
# y coincidir con el kernel del dispositivo (versión y configuración),
# o insmod fallará por vermagic/símbolos.

obj-m       += phmeop.o
phmeop-objs := entry.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: all clean
