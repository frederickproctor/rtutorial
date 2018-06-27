/*
  math_task.c

  Sets up a single periodic task that increments a floating point value
  by some small amount, computes the sine and cosine, and increments a
  cumulative count by sin^2 + cos^2 = 1, thus flexing the floating point
  unit (FPU). 

  The RT Linux functions for enabling and disabling the save/restore of the
  FPU are called. In the task, every second the FPU save/restore flag is
  alternately set and cleared. When cleared, the FPU registers are not
  saved, thus rendering all Linux process floating-point calculations
  useless. A companion process to this task runs and shows what this looks
  like: terrible floating-point results for one second, good ones for the
  next second, etc.

  Moral: you *must* enable the save/restore of the FPU in RT tasks that
  use it (e.g., if you have a 'double' anywhere, or call math library
  functions in <math.h>). 
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <rtai.h>
#include <rtai_sched.h>
#include <rtai_math.h>

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

static RT_TASK math_task;

static double cum = 0.0;	/* this gets incremented each cycle */

static int task_use_fpu = 1;	/* initially we enable FPU save/restore */

#define SQ(x) ((x)*(x))		/* how to square a number */

/*
  This task code runs at some nominal rate, unimportant, and increments
  the 'cum' cumulative count by 1. "1" here means computing the sine
  and cosine of some incrementally increasing number, and using the
  trig identity sin^2 + cos^2 = 1 to make "1". 

  Each second, the task alternately sets and clears the RT Linux flag
  for saving/restoring the floating point unit (FPU), by calling

  rt_task_use_fpu(rt_whoami(), flag)

  where 'flag' is 1 for saving/restoring, 0 for don't save/restore.
 */
static void task_code(int dummy)
{
  RTIME next;
  double arg;
  double increment;

  next = rt_get_time() + nano2count(1e9);
  arg = 0.0;
  increment = 0.01;		/* 0.01 radians, about half a degree */

  while (1) {
    if (rt_get_time() > next) {
      task_use_fpu = ! task_use_fpu;
      /*
	Here we toggle the saving/restoring of the FPU. When disabled,
	any RT or Linux task that uses floating point will be
	compromised.  You don't really want to do this!
       */
      rt_task_use_fpu(rt_whoami(), task_use_fpu);
      next += nano2count(1e9);
    }
    cum += (SQ(sin(arg)) + SQ(cos(arg)));
    arg += increment; 
    rt_task_wait_period();
  }

  return;
}

static double result;

int init_module(void)
{
  int retval;
  RTIME task_period;

  /*
    The call to 'rt_linux_use_fpu()' tells RT Linux that some Linux
    processes will be using the FPU. Unless you know in advance that
    no Linux processes will do this (e.g., Matlab, your code, a calculator),
    you should always call this with a '1' if you intend to use floating
    point in your real-time task.
   */
  rt_linux_use_fpu(1);

  /*
    Calculate some combination of math function to compare against
    known result to make sure the math functions are working properly.
   */
  result = sin(0.1) + cos(0.2) + atan2(0.3, 0.4) + log(0.5);
  if (fabs(result - 1.0302539227214) > 1.0e-6) {
    printk("math test failed\n");
  } else {
    printk("math test ok\n");
  }

  /*
    Note the passing of the 'task_use_fpu' flag here, which is initially
    1 to signify that we will be using floating point in this task.
    In the previous examples, we left this as 0 since we didn't use
    floating point.
   */
  retval = rt_task_init(&math_task, task_code, 0, 1024, RT_LOWEST_PRIORITY,
			task_use_fpu,	/* uses floating point unit */
			0);
  if (retval) {
    printk("could not init task\n");
    return retval;
  }

  /* run our task in one-shot mode */
  rt_set_oneshot_mode();
  start_rt_timer(1);
  task_period = nano2count(1e5);
  retval = rt_task_make_periodic(&math_task,
				 rt_get_time() + task_period,
				 task_period);
  if (retval) {
    printk("could not start task\n");
    return retval;
  }

  return 0;
}

void cleanup_module(void)
{
  rt_task_delete(&math_task);

  stop_rt_timer();

  return;
}
