all : apps modules

clean : apps_clean modules_clean

# this section is for building the application

apps : shm_app

shm_app : shm_core.c shm_app.c
	gcc -g -Wall -I/usr/realtime/include $^ -o $@

apps_clean :
	- rm -f shm_app

# this section is for building the kernel module

KERNEL_SOURCE_DIR = /usr/src/linux
EXTRA_CFLAGS += -I/usr/realtime/include -ffast-math -mhard-float -D__IN_RTAI__ -DEXPORT_SYMTAB

modules :
	$(MAKE) -C "$(KERNEL_SOURCE_DIR)" SUBDIRS="$(shell pwd)" $@

obj-m +=  shm_mod.o

shm_mod-objs := shm_core.o shm_task.o

modules_clean : 
	- rm -f *.o *.ko .*.cmd .*.flags *.mod.c Module.symvers
