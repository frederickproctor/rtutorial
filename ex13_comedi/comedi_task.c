/*
  comedi_task.c

  Demonstration of the Comedi interface to the built-in parallel port.

  We use the "kcomedilib" kernel-level interface to the convenient
  comedilib interface to the comedi drivers. 

  See the 'run' script for notes on how to set up the Comedi drivers
  and hook up your voltmeter.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <rtai.h>
#include <rtai_sched.h>
#include <linux/comedilib.h>	/* from /usr/local/comedi/include */

MODULE_LICENSE("GPL");

static RT_TASK comedi_task;	/* we'll fill this in with our task */
static RTIME comedi_period_ns = 1000000000; /* timer period, in nanoseconds */

void comedi_function(int arg)
{
  comedi_t *device;
  const char *device_name = "/dev/comedi0";
  unsigned int output_subdevice = 0;	/* 8 output bits */
  unsigned int input_subdevice = 1;	/* 5 input bits */
  unsigned int output_channel = 0;	/* D0 */
  unsigned int input_channel = 0;	/* C0 */
  unsigned int range = 0;
  unsigned int aref = AREF_GROUND;
  lsampl_t input;
  lsampl_t output = 0;
  unsigned char toggle = 0;

  device = comedi_open(device_name);
  if (NULL == device) {
    printk("comedi_task: can't open %s\n", device_name);
    return;
  }
  printk("comedi_task: got device: %s\n", comedi_get_board_name(device));
  
  while (1) {
    /* read the digital input from last cycle, which should have
       stabilized by now */
    if (-1 == comedi_data_read(device, input_subdevice, input_channel, range, aref, &input)) {
      printk("comedi_task: can't read input from %s\n", comedi_get_board_name(device));
      break;
    }

    /* print the input to the log if our period is 10 Hz or slower,
       noting that kernel printing doesn't support floating point
       numbers so we have to convert to an integer, and we'll multiply
       by 1000 to get some range */
    if (comedi_period_ns > 100000000) {
      printk("comedi_task: read %d milli-inputs\n", (int) (1000 * input));
    }

    /* write a digital output  */
    if (-1 == comedi_data_write(device, output_subdevice, output_channel, range, aref, output)) {
      printk("comedi_task: can't write output to %s\n", comedi_get_board_name(device));
      break;
    }
    toggle = ! toggle;
    output = toggle;
    rt_task_wait_period();
  }

  return;
}

int init_module(void)
{
  RTIME comedi_period_count;	/* requested timer period, in counts */
  RTIME timer_period_count;	/* actual timer period, in counts */
  int retval;			/* we look at our return values */

  rt_set_periodic_mode();
  comedi_period_count = nano2count(comedi_period_ns);
  timer_period_count = start_rt_timer(comedi_period_count);

  retval =
    rt_task_init(&comedi_task,	/* pointer to our task structure */
		 comedi_function, /* the actual timer function */
		 0,		/* initial task parameter; we ignore */
		 1024,		/* 1-K stack is enough for us */
		 RT_SCHED_LOWEST_PRIORITY, /* lowest is fine for our 1 task */
		 0,		/* uses floating point; we don't */
		 0);		/* signal handler; we don't use signals */
  if (0 != retval) {
    if (-EINVAL == retval) {
      /* task structure is already in use */
      printk("comedi_task: task structure already in use\n");
    } else if (-ENOMEM == retval) {
      /* stack could not be allocated */
      printk("comedi_task: can't allocate stack\n");
    } else {
      /* unknown error */
      printk("comedi_task: error initializing task structure\n");
    }
    return retval;
  }

  retval = 
    rt_task_make_periodic(&comedi_task, /* pointer to our task structure */
			  rt_get_time() + comedi_period_count, 
			  comedi_period_count); /* recurring period */
  if (0 != retval) {
    if (-EINVAL == retval) {
      /* task structure is already in use */
      printk("comedi_task: task structure is invalid\n");
    } else {
      /* unknown error */
      printk("comedi_task: error starting task\n");
    }
    return retval;
  }

  return 0;
}

void cleanup_module(void)
{
  int retval;

  retval = rt_task_delete(&comedi_task);

  if (0 !=  retval) {
    if (-EINVAL == retval) {
      /* invalid task structure */
      printk("comedi_task: task structure is invalid\n");
    } else {
      printk("comedi_task: error stopping task\n");
    }
  }

  return;
}

/*
  25 Pin D-subminiature parallel port connector pinout

  Pin	Pin Name	Pin Description and Function
  1 	/C0 	Output 9
  2 	D0 	Output 1
  3 	D1 	Output 2
  4 	D2 	Output 3
  5 	D3 	Output 4
  6 	D4 	Output 5
  7 	D5 	Output 6
  8 	D6 	Output 7
  9 	D7 	Output 8
  10 	S6	Input 4
  11 	/S7 	Input 5
  12 	S5	Input 3
  13 	S4 	Input 2
  14 	/C1 	Output 10
  15 	S3 	Input 1
  16 	C2 	Output 11
  17 	/C3 	Output 12
  18 	GND 	Strobe Ground
  19 	GND 	Output 1 and 2 Ground
  20 	GND 	Output 3 and 4 Ground
  21 	GND 	Output 5 and 6 Ground
  22 	GND 	Output 7 and 8 Ground
  23 	GND 	Busy and Fault Ground
  24 	GND 	Paper out, Select, and Acknowledge Ground
  25 	GND 	AutoFeed, Select input and Initialize Ground 
*/
