all : apps modules

clean : apps_clean modules_clean

apps modules apps_clean modules_clean :
	$(MAKE) -C ex01_periodic $@
	$(MAKE) -C ex02_twoper $@
	$(MAKE) -C ex03_variable $@
	$(MAKE) -C ex04_fifo $@
	$(MAKE) -C ex05_isr $@
	$(MAKE) -C ex06_shm $@
	$(MAKE) -C ex07_sem $@
	$(MAKE) -C ex08_rcservo $@
	$(MAKE) -C ex09_ledclock $@
	$(MAKE) -C ex10_stack $@
	$(MAKE) -C ex11_jitter $@
	$(MAKE) -C ex12_math $@
	$(MAKE) -C ex13_comedi $@
