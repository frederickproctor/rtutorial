/*
  periodic_task.c

  Sets up a task in pure periodic mode that toggles the speaker. In
  pure periodic mode, the timer is programmed once to expire at a
  given period, and all tasks must run at multiples of this period.
  This is useful when such a fundamental period can be identified,
  in which case the overhead of timer reprogramming is avoided.

  Compile with:

  gcc -c -I/usr/src/rtai/include -O2 periodic_task.c

  where -c means don't try to build a full executable (since we don't
  have a 'main'), -I/usr/src/rtai/include tells the compiler where
  to look for the real-time Linux headers for our RTAI flavor of RT Linux,
  and -O2 sets the optimization level that enables port I/O.
*/

/*
  RT Linux tasks are "kernel modules," meaning they run as if part of
  the privileged kernel, and can be loaded dynamically as modules rather
  than being compiled in to the kernel itself. We need to define the
  symbols __KERNEL__ and MODULE, and include the appropriate Linux
  header files, for this to happen.
 */
#include <linux/kernel.h>	/* decls needed for kernel modules */
#include <linux/module.h>	/* decls needed for kernel modules */
#include <linux/version.h>	/* LINUX_VERSION_CODE, KERNEL_VERSION() */

/*
  Specific header files for RTAI, our flavor of RT Linux
 */
#include "rtai.h"		/* RTAI configuration switches */
#include "rtai_sched.h"		/* rt_set_periodic_mode(), start_rt_timer(),
				   nano2count(), RT_LOWEST_PRIORITY,
				   rt_task_init(), rt_task_make_periodic() */

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
  Some RTAI functions return standard Linux symbolic error codes, so
  we'll include them
 */
#include <linux/errno.h>	/* EINVAL, ENOMEM */

/*
  Include declarations for inb() and outb(), the byte input and output
  functions for port I/O
 */
#include <asm/io.h>		/* may be <sys/io.h> on some systems */

/*
  Linux kernel modules in kernel versions 2.4 and later are asked to
  state their license terms.  "GPL" is the usual, for software
  released under the Gnu General Public License. The Linux kernel
  module loader 'insmod' will complain if it's anything else.
 */

/*
  THIS SOFTWARE WAS PRODUCED BY EMPLOYEES OF THE U.S. GOVERNMENT AS PART
  OF THEIR OFFICIAL DUTIES AND IS IN THE PUBLIC DOMAIN.

  When linked into the Linux kernel the resulting work is GPL. You
  are free to use this work under other licenses if you wish.
*/
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,0)
MODULE_LICENSE("GPL");
#endif

static RT_TASK sound_task;	/* we'll fill this in with our task */
static RTIME sound_period_ns = 1000000;	/* timer period, in nanoseconds */

#define SOUND_PORT 0x61		/* address of speaker */
#define SOUND_MASK 0x02		/* bit to set/clear */

/*
  sound_function() is our task code, executed each period. RTAI requires
  all tasks to be of this form, taking an int argument and returning nothing.
  We'll ignore our argument.

  This simple task just toggles the speaker port.
 */
void sound_function(int arg)
{
  unsigned char sound_byte;
  unsigned char toggle = 0;

  while (1) {
    /*
      Toggle the sound port
     */
    sound_byte = inb(SOUND_PORT);
    if (toggle) {
      sound_byte = sound_byte | SOUND_MASK;
    } else {
      sound_byte = sound_byte & ~SOUND_MASK;
    }
    outb(sound_byte, SOUND_PORT);
    toggle = ! toggle;

    /*
      Wait one period by calling

      void rt_task_wait_period(void);

      which applies to the currently executing task, and used the period
      set up in its task structure.
    */
    rt_task_wait_period();
  }

  /* we'll never get here */
  return;
}

/* 
   All Linux kernel modules must have 'init_module' as the entry point,
   similar to the C language 'main' for programs. init_module must return
   an integer value, 0 signifying success and non-zero signifying failure.

   The Linux kernel module installer program 'insmod' will look at this
   and print out an error message at the console.
*/
int init_module(void)
{
  RTIME sound_period_count;	/* requested timer period, in counts */
  RTIME timer_period_count;	/* actual timer period, in counts */
  int retval;			/* we look at our return values */

  /*
    Set up the timer to expire in pure periodic mode by calling

    void rt_set_periodic_mode(void);

    This sets the periodic mode for the timer. It consists of a fixed
    frequency timing of the tasks in multiple of the period set with a
    call to start_rt_timer. The resolution is that of the 8254
    frequency (1193180 hz). Any timing request not an integer multiple
    of the period is satisfied at the closest period tick. It is the
    default mode when no call is made to set the oneshot mode.
  */
  rt_set_periodic_mode();

  /*
    Start the periodic timer by calling

    RTIME start_rt_timer(RTIME period);

    This starts the timer with the period 'period' in internal count units.
    It's usually convenient to provide periods in second-like units, so
    we use the nano2count() conversion to convert our period, in nanoseconds,
    to counts. The return value is the actual period set up, which may
    differ from the requested period due to roundoff to the allowable
    chip frequencies.

    Look at the console, or /var/log/messages, to see the printk()
    messages.
  */
  sound_period_count = nano2count(sound_period_ns);
  timer_period_count = start_rt_timer(sound_period_count);
  printk("periodic_task: requested %d counts, got %d counts\n",
	    (int) sound_period_count, (int) timer_period_count);

  /*
    Initialize the task structure by calling

    rt_task_init(RT_TASK *task, void *rt_thread, int data, int stack_size,
    int priority, int uses_fpu, void *signal);

    This structure will be passed to rt_task_init() later to start the
    task.

    'task' is a pointer to an RT_TASK type structure whose space must
    be provided by the application. It must be kept during the whole
    lifetime of the real time task and cannot be an automatic
    variable.  'rt_thread' is the entry point of the task
    function. The parent task can pass a single integer value data to
    the new task.  'stack_size' is the size of the stack to be used by
    the new task. See the note below on computing stack size.
    'priority' is the priority to be given the task. The highest
    priority is 0, while the lowest is RT_LOWEST_PRIORITY (0x3fffFfff,
    or 1073741823).  'uses_fpu' is a flag. Nonzero value indicates
    that the task will use the floating point unit.  'signal' is a
    function that is called, within the task environment and with
    interrupts disabled, when the task becomes the current running
    task after a context switch.

    The newly created real time task is initially in a suspend
    state. It is can be made active either with
    rt_task_make_periodic(), rt_task_make_periodic_relative_ns() or
    rt_task_resume().
  */
  retval = 
    rt_task_init(&sound_task,	/* pointer to our task structure */
		 sound_function, /* the actual timer function */
		 0,		/* initial task parameter; we ignore */
		 1024,		/* 1-K stack is enough for us */
		 RT_LOWEST_PRIORITY, /* lowest is fine for our 1 task */
		 0,		/* uses floating point; we don't */
		 0);		/* signal handler; we don't use signals */
  if (0 != retval) {
    if (-EINVAL == retval) {
      /* task structure is already in use */
      printk("periodic task: task structure already in use\n");
    } else if (-ENOMEM == retval) {
      /* stack could not be allocated */
      printk("periodic task: can't allocate stack\n");
    } else {
      /* unknown error */
      printk("periodic task: error initializing task structure\n");
    }
    return retval;
  }

  /*
    Start the task by calling

    int rt_task_make_periodic (RT_TASK *task, RTIME start_time, RTIME period);

    This marks the task 'task', previously created with
    rt_task_init(), as suitable for a periodic execution, with period
    'period'.  The time of first execution is given by start_time.
    start_time is an absolute value measured in clock
    ticks. After the first task invocation, it should call
    rt_task_wait_period() to reschedule itself.
   */
  retval = 
    rt_task_make_periodic(&sound_task, /* pointer to our task structure */
			  /* start one cycle from now */
			  rt_get_time() + sound_period_count, 
			  sound_period_count); /* recurring period */
  if (0 != retval) {
    if (-EINVAL == retval) {
      /* task structure is already in use */
      printk("periodic task: task structure is invalid\n");
    } else {
      /* unknown error */
      printk("periodic task: error starting task\n");
    }
    return retval;
  }

  return 0;
}

/*
  Every Linux kernel module must define 'cleanup_module', which takes
  no argument and returns nothing.

  The Linux kernel module installer program 'rmmod' will execute this
  function.
 */
void cleanup_module(void)
{
  int retval;

  retval = rt_task_delete(&sound_task);

  if (0 !=  retval) {
    if (-EINVAL == retval) {
      /* invalid task structure */
      printk("periodic task: task structure is invalid\n");
    } else {
      printk("periodic task: error stopping task\n");
    }
  }

  /* turn off sound in case it was left on */
  outb(inb(SOUND_PORT) & ~SOUND_MASK, SOUND_PORT);

  return;
}
