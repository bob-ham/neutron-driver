obj-m := neutron.o
neutron-objs := main.o core.o

# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
    # Nothing extra needed here for simple module targets

else
    # Otherwise we're called directly from the command line;
    # determine the kernel version.
    KVERSION ?= $(shell uname -r)
    KDIR ?= /lib/modules/$(KVERSION)/build
    PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f compile_commands.json

endif