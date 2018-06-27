/*
  fifo_task.c

  Shows how to set up a FIFO data queue for sharing data between
  real-time tasks and user-level applications. The RT task creates
  two FIFOs, one for commands in from the user process and one for
  status back to the user process. As declared in common.h, there
  are three commands: turn the speaker on, turn it off, and set the
  frequency.

  The periodic task just toggles the speaker port and increments
  a heartbeat. Most of the work is done by a "handler" task that
  is called when the command FIFO is written by the user process.
  In the handler, we enable/disable the speaker using a shared global
  variable, and reset the period of the speaker task if requested.
  The handler also echoes the command and the heartbeat back to 
  the user process via the status FIFO.
*/

#include <linux/kernel.h>
#include <linux/module.h>
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

/*
  THIS SOFTWARE WAS PRODUCED BY EMPLOYEES OF THE U.S. GOVERNMENT AS PART
  OF THEIR OFFICIAL DUTIES AND IS IN THE PUBLIC DOMAIN.

  When linked into the Linux kernel the resulting work is GPL. You
  are free to use this work under other licenses if you wish.
*/
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,0)
MODULE_LICENSE("GPL");
#endif

static RT_TASK my_task;
static RTIME task_period;	/* used initially and reset by FIFO messages */
static int heartbeat;
static int freq;

static unsigned char enable_sound;
#define SOUND_PORT 0x61		/* address of speaker */
#define SOUND_MASK 0x02		/* bit to set/clear */

static void task_code(int arg)
{
  unsigned char sound_byte;
  unsigned char toggle = 0;

  enable_sound = 1;
  heartbeat = 0;
  freq = 100;

  while (1) {
    sound_byte = inb(SOUND_PORT);
    if (enable_sound && toggle) {
      sound_byte = sound_byte | SOUND_MASK;
    } else {
      sound_byte = sound_byte & ~SOUND_MASK;
    }
    outb(sound_byte, SOUND_PORT);
    toggle = ! toggle;
    heartbeat++;

    rt_task_wait_period();
  }

  return;
}

static int fifo_handler(unsigned int fifo)
{
  COMMAND_STRUCT command;
  STATUS_STRUCT status;
  int num;

  /*
    Read everything out of the fifo into our 'command' structure,
    which will hold the last one read. This will clear out all the
    commands, in case there are more than one queued.
  */
  do {
    num = rtf_get(RTF_COMMAND_NUM, &command, sizeof(command));
  } while (num != 0);

  /*
    Let's see what we got...
   */
  if (command.command == SOUND_ON) {
    enable_sound = 1;
  } else if (command.command == SOUND_OFF) {
    enable_sound = 0;
  } else if (command.command == SOUND_FREQ) {
    /*
      Here we change the period of the task by calling the familiar
      rt_task_make_periodic(), which we can do on-the-fly. The period
      is the inverse of the frequency, times 1e9 to convert to nanoseconds,
      so we want to ensure that the frequency is positive.
     */
    freq = command.freq > 0 ? command.freq : 1;
    task_period = 1e9 / freq;
    rt_task_make_periodic(&my_task,
			  rt_get_time(),
			  nano2count(task_period));
  }
  /* else unknown command, so ignore it */

  /* now echo what we did to the status fifo */
  status.command_num_echo = command.command_num;
  status.freq = freq;
  status.heartbeat = heartbeat;
  rtf_put(RTF_STATUS_NUM, &status, sizeof(status));

  return 0;
}

int init_module(void)
{
  int retval;

  /*
    Create two FIFOs, one for commands from the user  process to our
    RT task, and one from us to them. We call

    int rtf_create (unsigned int fifo, int size);

    rtf_create creates a real-time fifo (RT-FIFO) of initial size
    'size' and assigns it the identifier 'fifo'.  'fifo' is an
    integer, 0..63, that identifies the fifo on further
    operations. 'fifo' may refer an existing RT-FIFO. In this case the
    size is adjusted if necessary.

    The RT-FIFO is a mechanism, implemented as a character device, to
    communicate between real-time tasks and ordinary Linux processes. The
    rtf_* functions are used by the real-time tasks; Linux processes use
    standard character device access functions such as read, write, and
    select.

    RT-FIFO numbers 0..63 are associated with character devices
    /dev/rft0..ftf63. If you write to RT-FIFO 3, the user process will
    read from /dev/rtf3. 
  */
  
  retval = rtf_create(RTF_COMMAND_NUM, RTF_SIZE);
  if (retval) {
    printk("could not create RT-FIFO %d\n", RTF_COMMAND_NUM);
    return retval;
  }
  rtf_reset(RTF_COMMAND_NUM);	/* clear it out */

  retval = rtf_create(RTF_STATUS_NUM, RTF_SIZE);
  if (retval) {
    printk("could not create RT-FIFO %d\n", RTF_STATUS_NUM);
    return retval;
  }
  rtf_reset(RTF_STATUS_NUM);

  /*
    Associate our handler task with the command FIFO. When data is
    written to this FIFO by the user process, this handler will be
    called.
   */
  retval = rtf_create_handler(RTF_COMMAND_NUM, fifo_handler);
  if (retval) {
    printk("could not create RT-FIFO handler\n");
    return retval;
  }

  retval = rt_task_init(&my_task, task_code, 0, 1024,
			RT_LOWEST_PRIORITY, 0, 0);
  if (retval) {
    printk("could not init task\n");
    return retval;
  }

  /* run our sound task in one-shot mode, nominally at 100 Hz */
  rt_set_oneshot_mode();
  start_rt_timer(1);
  task_period = nano2count(1e7); /* start at 100 Hz */
  retval = rt_task_make_periodic(&my_task,
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
  rt_task_delete(&my_task);

  /*
    Remove our FIFOs
   */
  rtf_destroy(RTF_STATUS_NUM);
  rtf_destroy(RTF_COMMAND_NUM);

  stop_rt_timer();

  return;
}
