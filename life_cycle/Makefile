KDIR := /usr/src/linux-headers-$(shell uname -r)

PWD := $(shell pwd)

obj-m := cycle_de_vie.o

clean modules_install modules:
	make -C $(KDIR) M=$(PWD) $@
