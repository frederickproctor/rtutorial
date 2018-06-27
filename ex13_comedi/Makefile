all : apps modules

clean : apps_clean modules_clean

# this section is for building the application

# we have no application, so the rule is empty

apps apps_clean :

# this section is for building the kernel module

KERNEL_SOURCE_DIR = /usr/src/linux
EXTRA_CFLAGS += -I/usr/realtime/include -I/usr/local/comedi/include -ffast-math -mhard-float -D__IN_RTAI__ -DEXPORT_SYMTAB

modules :
	$(MAKE) -C "$(KERNEL_SOURCE_DIR)" SUBDIRS="$(shell pwd)" $@

obj-m := comedi_task.o

modules_clean : 
	- rm -f *.o *.ko .*.cmd .*.flags *.mod.c Module.symvers
