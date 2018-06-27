/*
  variable_task.c

  Sets up a task in one-shot mode that toggles the speaker. In
  one-shot mode, the timer is reprogrammed every task cycle.
  This is useful when no fundamental task period can be identified,
  but has the disadvantage of the recurring time to reprogram the
  8254 timer chip, about 2 microseconds.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/errno.h>	/* EINVAL, ENOMEM */
#include <asm/io.h>		/* inb(), outb(), may be <sys/io.h> */
#include "rtai.h"		/* RTAI configuration switches */
#include "rtai_sched.h"		/* rt_set_oneshot_mode(), start_rt_timer(),
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
  THIS SOFTWARE WAS PRODUCED BY EMPLOYEES OF THE U.S. GOVERNMENT AS PART
  OF THEIR OFFICIAL DUTIES AND IS IN THE PUBLIC DOMAIN.

  When linked into the Linux kernel the resulting work is GPL. You
  are free to use this work under other licenses if you wish.
*/
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,0)
MODULE_LICENSE("GPL");
#endif

static RT_TASK sound_task;	/* we'll fill this in with our task */
static RTIME sound_period_ns = 1e6;	/* initial period -> 1 kHz */
static RTIME increment_ns = 1e4; /* period change */

#define SOUND_PORT 0x61		/* address of speaker */
#define SOUND_MASK 0x02		/* bit to set/clear */

/*
  sound_function() linearly varies its period to sweep through a
  frequency range, beginning at 10 kHz and going down to about half and
  back up. Note that we expect an argument, the initial period, which
  will be provided by the rt_task_init() function when the task structure
  is set up.

  We will be calling

  void rt_sleep(RTIME delay)

  to put the task to sleep for 'delay' counts. We want 'delay' to be
  the intertask period, so we should adjust this by subtracting off
  the time already taken by the task before rt_sleep() is called to
  be the most accurate.
 */
void sound_function(int initial_period_count)
{
  unsigned char sound_byte;
  unsigned char toggle = 0;
  unsigned char freq_up = 0; /* lets us sweep frequency down and up */
  RTIME delay;			/* variable intertask period */
  RTIME increment_count;   /* how much the period changes per cycle */
  RTIME start;		   /* start time for task, for computing... */
  RTIME delta;			/* the elapsed time for task */

  /* Set our intertask delay to start at the initial period. We
     will increase this to sweep through frequencies. */
  delay = (RTIME) initial_period_count;
  increment_count = nano2count(increment_ns);

  while (1) {
    /* Record the start time of our code */
    start = rt_get_time();

    /* Toggle the sound port */
    sound_byte = inb(SOUND_PORT);
    if (toggle) {
      sound_byte = sound_byte | SOUND_MASK;
    } else {
      sound_byte = sound_byte & ~SOUND_MASK;
    }
    outb(sound_byte, SOUND_PORT);
    toggle = ! toggle;

    /* Vary the delay */
    if (freq_up) {
      /* decrease delay to increase frequency */
      delay -= increment_count;
      if (delay < initial_period_count) {
	delay = initial_period_count;
	freq_up = ! freq_up;
      }
    } else {
      /* increase delay to decrease frequency */
      delay += increment_count;
      if (delay > initial_period_count * 2) {
	delay = initial_period_count * 2;
	freq_up = ! freq_up;
      }
    }

    /*
      Compute the elapsed time for our code by subtracting start from end.
      No need to worry about rollover, since rt_get_time() returns an RTIME,
      a long long int, 64 bits, and a count is a CPU tick, say 32 bits per
      second on a 4 GHz machine. This gives about 2^31 seconds until rollover,
      or about 70 years.
    */
    delta = rt_get_time() - start;

    /*
      Wait some variable amount of time by calling

      void rt_sleep(RTIME delay)

      where 'delay' is the time, in counts, to wait. This is time
      from "now", not the start of the task. Subtract off the time
      taken for the task code to get a more accurate 'delay' between
      task invocations.
    */

    /*
      Compute period to sleep, which could be negative if 'delay'
      is less than 'delta'. Before we do a subtraction, check this
      since these values are unsigned and a negative value will be
      interpreted as a huge positive value.
     */
    if (delay <= delta) {
      /* just drop through without a wait */
    } else {
      rt_sleep(delay - delta);
    }
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
  int retval;			/* we look at our return values */

  /*
    Set up the timer to expire in one-shot mode by calling

    void rt_set_oneshot_mode(void);

    This sets the one-shot mode for the timer. The 8254 timer chip
    will be reprogrammed each task cycle, at a cost of about 2 microseconds.
  */
  rt_set_oneshot_mode();

  /*
    Start the one-shot timer by calling

    RTIME start_rt_timer(RTIME count)

    with 'count' set to 1 as a dummy nonzero value provided for the period since
    we don't have one. We habitually use a nonzero value since 0
    could mean disable the timer to some.
  */
  (void) start_rt_timer(1);

  /*
    Set the initial task period in RTIME count units. We'll pass this
    to the sound task as an argument, which it will then change as
    it sweeps down through a frequency range.
   */
  sound_period_count = nano2count(sound_period_ns);

  retval = 
    rt_task_init(&sound_task,	/* pointer to our task structure */
		 sound_function, /* the actual timer function */
		 sound_period_count, /* initial task parameter */
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
    Start the RT task with rt_task_make_periodic(), as usual; here
    the period is the default used. We'll start with this and change
    it each cycle.
   */
  retval = 
    rt_task_make_periodic(&sound_task, /* pointer to our task structure */
			  /* start one period from now */
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

  return 0;			/* success! */
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
