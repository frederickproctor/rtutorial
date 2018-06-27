/*
  sem_task.c

  Set up two periodic tasks that share a large array, and use a semaphore
  to ensure that there are no simultaneous access by the low-priority
  writer and the high-priority reader.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/errno.h>	/* ENOMEM */
#include <linux/moduleparam.h>
#include "rtai.h"
#include "rtai_sem.h"		/* SEM, rt_sem_init() */
#include "rtai_sched.h"		/* sched stuff plus sem stuff for RTAI < 3 */

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

RT_TASK fast_task;
RT_TASK slow_task;

static RTIME period_ns = 100000; /* 100 microsecond base period */

static RTIME fast_period_count;
static RTIME slow_period_count;

/*
  SEM is the RTAI semaphore structure. We'll use one for the shared
  data structure.
 */
static SEM sem;

enum {SLOW_DELAY = 10};

static int read_count = 0;
static int write_count = 0;
static int bad_count = 0;

enum {ARRAY_SIZE = 1000000};
static int array[ARRAY_SIZE] = {0};

/*
  The DO_SEM flag tells us whether to use semaphores or not. Presumably
  we will see bad behavior when we don't use them. This flag can be 
  set at run time (insmod time). See MODULE_PARM below.
 */

int DO_SEM = 0;
module_param(DO_SEM, int, 0);

static void fast_function(int arg)
{
  while (1) {
    /*
      Bracket "critical section" reading of shared data structure
      with semaphore take/give.
    */
    if (DO_SEM) {
      rt_sem_wait(&sem);
    }
    if (array[0] != array[ARRAY_SIZE - 1]) {
      bad_count++;
    }
    if (DO_SEM) {
      rt_sem_signal(&sem);
    }

    read_count++;

    rt_task_wait_period();
  }

  return;
}

static void slow_function(int arg)
{
  RTIME start, end, diff;
  int count = 0;		/* what we fill the array with */
  int t;

  while (1) {
    /*
      Bracket critical section writing of shared data structure
      with semaphore take/give.
     */
    count++;
    if (DO_SEM) {
      rt_sem_wait(&sem);
    }
    start = rt_get_time();
    for (t = 0; t <  ARRAY_SIZE; t++) {
      array[t] = count;
    }
    end = rt_get_time();
    if (DO_SEM) {
      rt_sem_signal(&sem);
    }

    /*
      Check the time it takes to set the array, and if it takes longer
      than our period, make us run slower. Otherwise this task locks
      up the CPU.
     */
    if (end > start) {
      diff = end - start;
    } else {
      diff = slow_period_count;
    }
    if (diff > slow_period_count) {
      slow_period_count += diff;
      rt_task_make_periodic(rt_whoami(), end + slow_period_count,
			    slow_period_count);
    }

    write_count++;

    rt_task_wait_period();
  }

  return;
}

int init_module(void)
{
  fast_period_count = nano2count(period_ns);
  slow_period_count = SLOW_DELAY * fast_period_count;
  rt_set_periodic_mode();
  start_rt_timer(fast_period_count);

  /*
    Set up the semaphore by calling

    void rt_sem_init(SEM * sem, int value);

    where 'value' is 1 so that the first call to rt_sem_wait()
    by the other tasks will return, allowing the first access.
   */
  rt_sem_init(&sem, 1);	/* FIXME */

  /*
    Start the fast task at the next-lowest priority.
   */
  (void) rt_task_init(&fast_task, fast_function, 0, 1024,
		      RT_LOWEST_PRIORITY - 1, 0, 0);
  rt_task_make_periodic(&fast_task, rt_get_time() + fast_period_count, 
			fast_period_count);

  /*
    Start the slow task at the lowest priority.
  */
  (void) rt_task_init(&slow_task, slow_function, 0, 1024,
		      RT_LOWEST_PRIORITY, 0, 0);
  rt_task_make_periodic(&slow_task, rt_get_time() + slow_period_count,
			slow_period_count);

  return 0;
}

void cleanup_module(void)
{
  rt_task_delete(&slow_task);
  rt_task_delete(&fast_task);

  rt_sem_delete(&sem);

  if (DO_SEM) {
    printk("with sem: reads/writes/bad reads = %d/%d/%d\n",
	   read_count, write_count, bad_count);
  } else {
    printk("no sem: reads/writes/bad counts = %d/%d/%d\n",
	   read_count, write_count, bad_count);
  }

  return;
}
