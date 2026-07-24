TARGET = kekeos-mod.ko

obj-m += kekeos-mod.o
kekeos-mod-objs := kekeos_dev.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M="$(PWD)" modules

clean:
	$(MAKE) -C $(KDIR) M="$(PWD)" clean

install:
	cp $(TARGET) ../../build/
