/*
  twoper_task.c

  Sets up two tasks in pure periodic mode. The first runs at 10 kHz
  and toggles the speaker with a square wave based on a duration set
  by the second task. The second task changes the duration every second. 
*/

/*
  The following defines and includes were described in ex1_periodic
  and won't be described again. Ditto for other concepts previously
  described.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <asm/io.h>		/* may be <sys/io.h> on some systems */

#include "rtai.h"
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
  THIS SOFTWARE WAS PRODUCED BY EMPLOYEES OF THE U.S. GOVERNMENT AS PART
  OF THEIR OFFICIAL DUTIES AND IS IN THE PUBLIC DOMAIN.

  When linked into the Linux kernel the resulting work is GPL. You
  are free to use this work under other licenses if you wish.
*/
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,0)
MODULE_LICENSE("GPL");
#endif

static RT_TASK sound_task;
static RT_TASK delay_task;
static RTIME sound_period_ns = 1e5;	/* in nanoseconds, -> 10 kHz */
static RTIME delay_period_ns = 1e9;	/* in nanoseconds, -> 1 Hz */

#define SOUND_PORT 0x61		/* address of speaker */
#define SOUND_MASK 0x02		/* bit to set/clear */

/*
  delay_count is an integer shared between the two tasks that sets the
  duration of the square wave. The sound task will add this many of
  its cycles between toggling the speaker port. If delay_count is 0,
  the speaker is toggled at the full rate.  Larger values of
  delay_count will reduce the audible frequency.
 */
static int delay_count = 2;

 /*
   sound_function() toggles the speaker port at a fixed period and
   a variable duration. The duration is set in another task.
  */
 void sound_function(int arg)
 {
   int delay_left = delay_count;	/* decremented each cycle */
   unsigned char sound_byte;
   unsigned char toggle = 0;

   while (1) {
     /*
       Check for any remaining delay. If none is remaining, it's time
       to toggle the speaker port. Otherwise decrement our remaining
       delay and wait for the next time.
     */
     if (delay_left > 0) {
       /* still some delay left */
       delay_left--;
     } else {
       /* else it's time to toggle */
       sound_byte = inb(SOUND_PORT);
       if (toggle) {
	 sound_byte = sound_byte | SOUND_MASK;
       } else {
	 sound_byte = sound_byte & ~SOUND_MASK;
       }
       outb(sound_byte, SOUND_PORT);
       toggle = ! toggle;
       delay_left = delay_count;	/* reload our delay */
     }

     rt_task_wait_period();
   }

   /* we'll never get here */
   return;
 }

 /*
   delay_function() changes the value of the delay_count shared variable,
   which sets the period of the sound task's square wave.
  */
 void delay_function(int arg)
 {
   while (1) {
     delay_count++;
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
  RTIME sound_period_count;
  RTIME delay_period_count;
  RTIME timer_period_count;
  int retval;			

  rt_set_periodic_mode();

  /*
    Set the timer period to be that of the sound task, the faster of
    the two
   */
  sound_period_count = nano2count(sound_period_ns);
  delay_period_count = nano2count(delay_period_ns);
  timer_period_count = start_rt_timer(sound_period_count);
  printk("sound task: requested %d counts, got %d counts\n",
	    (int) sound_period_count, (int) timer_period_count);

  /* set up the sound task structure */
  retval = 
    rt_task_init(&sound_task,	/* pointer to our task structure */
		 sound_function, /* the actual timer function */
		 0,		/* initial task parameter; we ignore */
		 1024,		/* 1-K stack is enough for us */
		 RT_LOWEST_PRIORITY - 1, /* make this priority higher */
		 0,		/* uses floating point; we don't */
		 0);		/* signal handler; we don't use signals */
  if (0 != retval) {
    if (-EINVAL == retval) {
      /* task structure is already in use */
      printk("sound task: task structure already in use\n");
    } else if (-ENOMEM == retval) {
      /* stack could not be allocated */
      printk("sound task: can't allocate stack\n");
    } else {
      /* unknown error */
      printk("sound task: error initializing task structure\n");
    }
    return retval;
  }

  /* set up the delay task structure */
  retval = 
    rt_task_init(&delay_task,	/* pointer to our task structure */
		 delay_function, /* the actual timer function */
		 0,		/* initial task parameter; we ignore */
		 1024,		/* 1-K stack is enough for us */
		 RT_LOWEST_PRIORITY, /* make this priority lower */
		 0,		/* uses floating point; we don't */
		 0);		/* signal handler; we don't use signals */
  if (0 != retval) {
    if (-EINVAL == retval) {
      /* task structure is already in use */
      printk("delay task: task structure already in use\n");
    } else if (-ENOMEM == retval) {
      /* stack could not be allocated */
      printk("delay task: can't allocate stack\n");
    } else {
      /* unknown error */
      printk("delay task: error initializing task structure\n");
    }
    return retval;
  }

  /* run the sound task */
  retval = 
    rt_task_make_periodic(&sound_task, /* pointer to our task structure */
			  /* start one period from now */
			  rt_get_time() + sound_period_count,
			  sound_period_count); /* recurring period */
  if (0 != retval) {
    if (-EINVAL == retval) {
      /* task structure is already in use */
      printk("sound task: task structure is invalid\n");
    } else {
      /* unknown error */
      printk("sound task: error starting task\n");
    }
    return retval;
  }

  /* run the delay task */
  retval = 
    rt_task_make_periodic(&delay_task, /* pointer to our task structure */
			  /* start one period from now */
			  rt_get_time() + delay_period_count,
			  delay_period_count); /* recurring period */
  if (0 != retval) {
    if (-EINVAL == retval) {
      /* task structure is already in use */
      printk("delay task: task structure is invalid\n");
    } else {
      /* unknown error */
      printk("delay task: error starting task\n");
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
      printk("sound task: task structure is invalid\n");
    } else {
      printk("sound task: error stopping task\n");
    }
  }

  retval = rt_task_delete(&delay_task);

  if (0 !=  retval) {
    if (-EINVAL == retval) {
      /* invalid task structure */
      printk("delay task: task structure is invalid\n");
    } else {
      printk("delay task: error stopping task\n");
    }
  }

  /* turn off sound in case it was left on */
  outb(inb(SOUND_PORT) & ~SOUND_MASK, SOUND_PORT);

  return;
}
