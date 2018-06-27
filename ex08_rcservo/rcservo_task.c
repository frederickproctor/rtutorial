/*
  rcservo_task.c

  Control two radio-controlled servo motors through the parallel port.

  Notes:

  At 5 V input, need all of 20K pull-up.

  Min time is 0.35 msec, max is 2.45 msec.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <asm/io.h>
#include "rtai.h"
#include "rtai_sched.h"
#include "rtai_fifos.h"
#include "common.h"

/*
  Some newer versions define RT_SCHED_LOWEST_PRIORITY instead, so we'll
  get that if necessary
 */
#if ! defined(RT_LOWEST_PRIORITY)
#if defined(RT_SCHED_LOWEST_PRIORITY)
#define RT_LOWEST_PRIORITY RT_SCHED_LOWEST_PRIORITY
#else
#error RT_SCHED_LOWEST_PRIORITY not defined
#endif
#endif

#define LPT_PORT 	0x378
#define LPT_CTRL	((LPT_PORT)+2)

#define RC_NUM 3
static RT_TASK up_task;
static RT_TASK down_task[RC_NUM];
static RTIME up_period[RC_NUM];
#define STACKSIZE 1024
#define FIFOSIZE 1024

static void up_func(int arg)
{
  RTIME now;
  int t;

  while (1) {
    now = rt_get_time();
    /*
      We write all bits, so we don't need to take care to leave
      some untouched.
     */
    outb(0x00, LPT_PORT);

    /*
      Schedule the other tasks that will write their individual bits.
      The start time is important, the period won't be used so we
      set it to a dummy nonzero value.
     */
    for (t = 0; t < RC_NUM; t++) {
      rt_task_make_periodic(&down_task[t], now + up_period[t], 1);
    }

    rt_task_wait_period();
  }

  return;
}

static void down_func(int which)
{
  while (1) {
    /*
      We write one bit, so we need to take care and leave others
      untouched. We do this by reading in the byte, setting our bit,
      and writing it back out. We run the risk of getting interrupted
      between our read and write, which we eliminate by running all
      these tasks at the same priority.
     */
    outb(inb(LPT_PORT) | (0x01 << which), LPT_PORT);

    rt_task_suspend(rt_whoami());
  }

  return;
}

/* maps integer x in the range [a..b] to [c..d] */
static int range_map(int a, int b, int c, int d, int x)
{
  if (a == b) return 0;

  return c + (d - c)*(x - a)/(b - a);
}

static int fifo_handler(unsigned int fifo)
{
  COMMAND_STRUCT command;
  int num;

  /*
    Read out everything from the FIFO into 'command', which
    will hold the last one.
  */
  do {
    num = rtf_get(0, &command, sizeof(command));
  } while (num != 0);

  /*
    Clamp command parameters to allowable range.
  */

  if (command.which < 0) command.which = 0;
  else if (command.which >= RC_NUM) command.which = RC_NUM;

  if (command.position < -1000) command.position = -1000;
  else if (command.position > 1000) command.position = 1000;

  /*
    Map positions in [-1000, 1000] to times in [350, 2450] microseconds,
    the range for the Futaba S3003 RC servos, which then need to be 
    multiplied by 1000 for nanoseconds.
   */

  /*
    Since 'up_period[]' is a shared global variable, we should consider
    whether this needs mutual exclusion protection. These are small data,
    type RTIME, which are long long integers, 64 bits. These are operated
    on atomically on Pentiums, but not 486s. Here we'll assume it's OK.
    Who's running 486s anymore?
   */  
  up_period[command.which] = 
    nano2count(1000 * range_map(-1000, 1000, 350, 2450, command.position));

  return 0;
}

/*
  THIS SOFTWARE WAS PRODUCED BY EMPLOYEES OF THE U.S. GOVERNMENT AS PART
  OF THEIR OFFICIAL DUTIES AND IS IN THE PUBLIC DOMAIN.

  When linked into the Linux kernel the resulting work is GPL. You
  are free to use this work under other licenses if you wish.
*/
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,0)
MODULE_LICENSE("GPL");
#endif

int init_module(void)
{
  int retval;
  int t;
  RTIME down_period;		/* 20 millisecond baseline period */

  rt_set_oneshot_mode();
  start_rt_timer(1);
  
  retval = rtf_create(0, FIFOSIZE);
  if (retval) {
    printk("could not create RT-FIFO\n");
    return retval;
  }
  rtf_reset(0);

  retval = rtf_create_handler(0, fifo_handler);
  if (retval) {
    printk("could not create RT-FIFO handler\n");
    return retval;
  }

  retval = rt_task_init(&up_task, up_func, 0, STACKSIZE,
			RT_LOWEST_PRIORITY, 0, 0);
  if (retval) {
    printk("can't create up task\n");
    return retval;
  }

  for (t = 0; t < RC_NUM; t++) {
    up_period[t] = nano2count(1000000);
    retval = rt_task_init(&down_task[t], down_func,
			  t,	/* give each task a unique ID */
			  STACKSIZE, RT_LOWEST_PRIORITY, 0, 0);
    if (retval) {
      printk("can't create up task %d\n", t);
      return retval;
    }
  }

  /*
    Start just the up task. This will schedule the down tasks.
   */
  down_period = nano2count(20000000);
  retval = rt_task_make_periodic(&up_task,
				 rt_get_time() + down_period,
				 down_period);
  if (retval) {
    printk("can't start up task\n");
    return retval;
  }

  return 0;
}

void cleanup_module(void)
{
  int t;

  /*
    Delete the up task first, since this schedules the down tasks and
    if we delete the down tasks first, the up task may reschedule them
    before we are able to delete it.
   */
  rt_task_delete(&up_task);

  for (t = 0; t < RC_NUM; t++) {
    rt_task_delete(&down_task[t]);
  }

  rtf_destroy(0);

  return;
}
