KDIR := /usr/src/linux-headers-$(shell uname -r)

PWD := $(shell pwd)

obj-m := pingpong_correction.o

clean modules_install modules:
	make -C $(KDIR) M=$(PWD) $@
