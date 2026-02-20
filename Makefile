obj-m := vtfs.o
vtfs-objs := src/iops.o src/fops.o src/vtfs.o src/super.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f Module.symvers modules.order

modules_install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install