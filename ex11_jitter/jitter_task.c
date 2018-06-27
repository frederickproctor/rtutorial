/*
  jitter_task.c

  Sets up a task in pure periodic mode that reads the Pentium time stamp
  counter and logs the readings into shared memory for later analysis.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include "rtai.h"
#include "rtai_sched.h"
#include "rtai_shm.h"
#include "tsc.h"		/* TSC structure, get_tsc() */
#include "common.h"		/* SHM_KEY, SHM_HOWMANY */

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

/*
  THIS SOFTWARE WAS PRODUCED BY EMPLOYEES OF THE U.S. GOVERNMENT AS PART
  OF THEIR OFFICIAL DUTIES AND IS IN THE PUBLIC DOMAIN.

  When linked into the Linux kernel the resulting work is GPL. You
  are free to use this work under other licenses if you wish.
*/
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,0)
MODULE_LICENSE("GPL");
#endif

static RT_TASK jitter_task;	/* we'll fill this in with our task */
static RTIME jitter_period_ns = PERIOD_NSEC; /* timer period, in nanoseconds */

static TSC * tsc_array = 0;

/*
  jitter_function() is our task code, which reads the time stamp counter
  and logs it into the shared memory buffer until it's full.
 */
void jitter_function(int arg)
{
  int index;

  index = 0;
  while (1) {
    /* inhibit logging if we're full */
    if (index < SHM_HOWMANY) {
      get_tsc(&tsc_array[index]); /* read time stamp into the log */
      index++;			/* move index to next one */
    }
    rt_task_wait_period();	/* and wait */
  }

  return;
}

int init_module(void)
{
  int retval;
  int t;
  RTIME jitter_period_count;
  TSC zero_tsc = {0, 0};

  /* get shared memory and fill it with the zero timestamp */
  tsc_array = rtai_kmalloc(SHM_KEY, SHM_HOWMANY * sizeof(TSC));
  if (0 == tsc_array) {
    return -ENOMEM;
  }
  for (t = 0; t < SHM_HOWMANY; t++) {
    tsc_array[t] = zero_tsc;
  }

  rt_set_periodic_mode();
  jitter_period_count = nano2count(jitter_period_ns);
  start_rt_timer(jitter_period_count);
  retval =  rt_task_init(&jitter_task, jitter_function, 0, 1024,
			 RT_LOWEST_PRIORITY, 0, 0);
  if (0 != retval) {
    return retval;
  }

  retval = rt_task_make_periodic(&jitter_task, 
				 rt_get_time() + jitter_period_count,
				 jitter_period_count);
  if (0 != retval) {
    return retval;
  }

  return 0;
}
void cleanup_module(void)
{
  rt_task_delete(&jitter_task);

  rtai_kfree(SHM_KEY);

  return;
}
