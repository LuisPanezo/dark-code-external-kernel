# phmeop — módulo de kernel Linux (/dev/phmeop)
#
# Objetos:
#   entry.o    driver (registro, open/release, ioctl)
#   resolve.o  resolución en runtime de símbolos no exportados (GKI)
#
# ---- GKI / Android arm64 (RedMagic 10 Pro, android15-6.6) ----
# En WSL2, con el árbol GKI ya compilado:
#   make ARCH=arm64 LLVM=1 LLVM_IAS=1 KDIR=/ruta/al/out/virtual_device_aarch64/common
#
# ---- Linux de escritorio (prueba de lógica, símbolos directos) ----
#   make PHMEOP_DIRECT=1
#
# Con PHMEOP_DIRECT=1 se usan las llamadas directas a los símbolos del kernel
# (solo funciona donde están exportados, p. ej. un kernel de escritorio).
# Sin él, el driver resuelve los símbolos en runtime (necesario en GKI).

obj-m       += phmeop.o
phmeop-objs := entry.o resolve.o

ifneq ($(PHMEOP_DIRECT),)
ccflags-y += -DPHMEOP_DIRECT
endif

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: all clean
