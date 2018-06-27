/*
  ledclock_task.c

  Adapted from software written by Stuart Hughes, formerly of Lineo, Inc.,
  shughes@lineo.com, seh@zee2.com. Released under the Gnu General Public
  License.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <asm/io.h>
#include "rtai.h"
#include "rtai_sched.h"
#include "rtai_fifos.h"

/*
  THIS SOFTWARE WAS PRODUCED BY EMPLOYEES OF THE U.S. GOVERNMENT AS PART
  OF THEIR OFFICIAL DUTIES AND IS IN THE PUBLIC DOMAIN.

  When linked into the Linux kernel the resulting work is GPL. You
  are free to use this work under other licenses if you wish.
*/
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,0)
MODULE_LICENSE("GPL");
#endif

#define PARPORT_BASE_ADDRESS 0x378
#define PARPORT_CTRL_ADDRESS (PARPORT_BASE_ADDRESS + 2)
#define PARPORT_IRQ 7

#define NCHARS		16

#define BASE_PER (1e9 / 7000)	/* 142857 */

struct data_ {
  int irq;
  int count;
  int lpt_ctrl;
  long long t_diff;
  long long t_prev;
  long long t_short;
  long long t_long;
  char pattern[1024];

} d = {7, 4, 0, 0, 0, 0, 0, {0xff, 0x00, 0xff, 0x00}};

RT_TASK master_task;

static void disable_parport_int(void)
{
  /* clear bit 4 of the parallel port control register, two bytes up
     from the base address */
  outb(inb(PARPORT_BASE_ADDRESS+2) & ~0x10, PARPORT_BASE_ADDRESS+2);
}

static void enable_parport_int(void)
{
  /* set bit 4 of the parallel port control register, two bytes up
     from the base address */
  outb(inb(PARPORT_BASE_ADDRESS+2) | 0x10, PARPORT_BASE_ADDRESS+2);
}

static void master_function(int arg)
{
  int i, j, cnt, os, cyc;
  char *ptr;

  os = 0;

  while (1) {
    rt_task_suspend(&master_task);

    /*
      Set the start time based on which tempo we're on, and use BASE_PER
      as the inter-column period.
     */
    rt_task_make_periodic(&master_task, nano2count(d.t_long + 34000000), nano2count(BASE_PER));
    cnt = d.count;
    if (cnt > NCHARS * 8) {
      cyc = 1;
      os++;
      if (os > cnt) {
	os = 0;
      }
    } else {
      cyc = 0;
      os = 0;
    }
    ptr = &d.pattern[os];

    for (i = 0; i < NCHARS; i++) {
      for (j = 0; j < 16; j++) {
	if (cyc && (ptr > &d.pattern[cnt])) {
	  ptr = d.pattern;
	}
	rt_task_wait_period();
	if (j % 2) {
	  outb(~*ptr++, PARPORT_BASE_ADDRESS);
	} else {
	  outb(0xff, PARPORT_BASE_ADDRESS);
	}
      }
    }
    outb(0xff, PARPORT_BASE_ADDRESS);
  }

  return;
}

int fifo_handler(unsigned int fifo)
{
  memset(d.pattern, 0, sizeof(d.pattern));

  d.count = rtf_get(fifo, d.pattern, sizeof(d.pattern));

  return 0;
}

static int count = 0;

static void slave_isr(void)
{
  long long now;

  disable_parport_int();

  count++;

  /* you get 2 pulse close together (42.5), the pattern repeats every
     80ms, this is used to get the direction of travel */
  now = rt_get_time_ns();
  d.t_diff = now - d.t_prev;
  d.t_prev = now;
  if (d.t_diff < 50000000LL) {
    d.t_short = now;
  } else {
  d.t_long = now;
  rt_task_resume(&master_task);
  }

  rt_startup_irq(PARPORT_IRQ);
  enable_parport_int();
}

int init_module(void)
{
  int retval;

  retval = rtf_create(0, 1024);
  if (retval) {
    printk("can't create fifo\n");
    return retval;
  }

  retval = rtf_create_handler(0, &fifo_handler);
  if (retval) {
    printk("can't create fifo handler\n");

    return retval;
  }

  /* create irq handler */
  rt_free_global_irq(PARPORT_IRQ);
  retval = rt_request_global_irq(PARPORT_IRQ, slave_isr);
  if (retval) {
    printk("can't attach to IRQ\n");
    return retval;
  }
  rt_startup_irq(PARPORT_IRQ);
  enable_parport_int();

  /* clear the leds */
  outb(0xFF, PARPORT_BASE_ADDRESS);

  retval = rt_task_init(&master_task, master_function, 0, 
			3000, 10, 0, 0);
  if (retval) {
    printk("can't initialize master task\n");
    return retval;
  }

  rt_set_oneshot_mode();
  start_rt_timer(1);

  retval = rt_task_make_periodic(&master_task, rt_get_time(), 1);
  if (retval) {
    printk("can't start master task\n");
    return retval;
  }

  return 0;
}

void cleanup_module(void)
{
  rt_task_delete(&master_task);
  disable_parport_int();
  rt_shutdown_irq(PARPORT_IRQ);
  rt_free_global_irq(PARPORT_IRQ);
  rtf_destroy(0);

  printk("count = %d\n", count);

  return;
}
