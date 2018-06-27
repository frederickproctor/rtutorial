all : apps modules

clean : apps_clean modules_clean

# this section is for building the application

apps : fifo_app

fifo_app : fifo_app.c
	gcc -g -Wall $< -o $@

apps_clean :
	- rm -f fifo_app

# this section is for building the kernel module

KERNEL_SOURCE_DIR = /usr/src/linux
EXTRA_CFLAGS += -I/usr/realtime/include -ffast-math -mhard-float -D__IN_RTAI__ -DEXPORT_SYMTAB

modules :
	$(MAKE) -C "$(KERNEL_SOURCE_DIR)" SUBDIRS="$(shell pwd)" $@

obj-m := fifo_task.o

modules_clean : 
	- rm -f *.o *.ko .*.cmd .*.flags *.mod.c Module.symvers
