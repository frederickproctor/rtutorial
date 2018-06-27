/*
  stack_task.c

  Shows how to determine the stack size for a task, using our own 
  functions rt_task_stack_init() and rt_task_stack_check().

  The stack size for a task is the space needed for its arguments, its
  stack variables, and all the variables encountered through the
  deepest series of functions it may call during the life of the
  task, including any padding for alignment of data.
    
  To get an accurate measure of the necessary stack depth requires a
  detailed analysis of these functions, and if we ever encounter
  something for which we have no source code, we have to guess. The
  compiler also introduces some uncertainty since it may overlap
  stacks if it determines that a function has finished referencing
  some of its stack before it calls another function. This means that
  in practice analytical stack size determination involves detailed
  detective work.

  One technique to quickly estimate stack size is to initialize the
  stack with some recognizable pattern, run the task for a while, and
  look at how much of the pattern remains. That's the unused stack.
  Often this pattern is 0xDEADBEEF, a clever hex integer that is
  easily visible in hex memory dump utilities.

  To apply this technique to RTAI, we need to know how the stack is
  allocated and initialized, so we don't clobber anything RTAI has
  set up for us on our stack. 

  In RTAI, a task's stack is allocated and initialized in rt_task_init():

  rt_task_init(RT_TASK *task, void *rt_thread, int data, int stack_size,
  int priority, int uses_fpu, void *signal);

  The RT_TASK structure contains, among other things, the pointers to the
  top and bottom of the stack:

  typedef struct rt_task_struct {
    ...
    int *stack;
    int *stack_bottom;
    ...
  }

  Inspecting the RTAI source code, specifically rtai_sched.c and 
  asm/rtai_sched.h, we can see the details of how RTAI sets up the
  task stack.

  In rt_task_init(), RTAI allocates 'stack_size' bytes with
  sched_malloc(), and sets the bottom of the stack,
  task->stack_bottom, to this memory.  The top of the stack,
  task->stack, is set to the bottom plus stack_size bytes minus 16
  bytes, rounded down to the nearest integer boundary. 5 integers are
  then pushed onto the stack. The top integer (4 bytes) of the stack
  is set to 0. The next integer down is set to the initial 'data'
  argument for the task. The next integer down is set to the
  'rt_thread' code for the task. The next integer down is set to
  0. The next integer down is set to some RTAI startup code, and the
  stack is left pointing to this. When initializing the stack, we must
  stop short of this.
  
  In our examples, we start with a stack size of 1K. If this is not enough,
  we will have strange problems which defy debugging, at which point we
  can try increasing stack size and hope the problem goes away. 

  With the program below, the hex output (from the console, or
  /var/log/messages) shows the part of the stack used by our code when
  it is stopped:

  DCC2034C: 76
  DCC20348: 75
  DCC20344: 74
  DCC20340: 73
  DCC2033C: 72
  DCC20338: 71
  DCC20334: 70
  DCC20330: 6F
  DCC2032C: 6E
  DCC20328: 6D
  DCC20324: 6C
  DCC20320: 6B
  DCC2031C: 6A
  DCC20318: 69
  DCC20314: 68
  DCC20310: 67
  DCC2030C: 66
  DCC20308: 65
  DCC20304: 64
  DCC20300: DEADBEEF
  ...
  DCC20020: DEADBEEF
  740 unused stack bytes

  The bottom of the stack is at address 0xDCC20020, and it grows upward
  toward 0xDCC2034C, which is close to the top of the stack. Here we see
  the unused stack portion at the bottom, from integers at 0xDCC20020
  through 0xDCC20300, inclusive, or 740 bytes. This is corroborated by
  a count made from the bottom of the stack up to the first non-DEADBEEF
  occurrence.
  
  The problem with this technique is that all the code paths may not
  be exercised during the run, and later your code may enter a deeper
  area that was not entered before. Since there is no performance impact
  on the task with this technique, it may be wise to leave the stack
  initialization and checking intact in init_module() and cleanup_module(),
  respectively.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/sched.h>
#include "rtai.h"
#include "rtai_sched.h"

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

static RT_TASK stack_task;
static int stack_task_inited;
#define STACKSIZE 1024
static int pattern = 0xDEADBEEF;

/* macro for getting the number of array elements of an array */
#define numof(a) (sizeof(a)/sizeof(a[0]))

/*
  some_function() uses the stack, putting numbers we could try and
  spot later on in the stack dump. We make sure and return a value
  that depends on the stack, so that it doesn't get optimized out
  and confuse us later. Optimization is a good way to reduce your
  stack needs; if you use it and trim your stack down, make sure it
  remains enabled.
 */
static int some_function(int arg)
{
  int t;
  int data[32];
  int val;

  val = 0;
  for (t = 0; t < numof(data); t++) {
    data[t] = arg + t;
    val += data[t];
  }

  return val;
}

static void task_code(int arg)
{
  /*
    Here is some more stack data.
   */
  int t;
  int data[16];
  int val;

  /* compute something for 'val' that makes sure we use all our stack */
  val = 0;
  for (t = 0; t < numof(data); t++) {
    data[t] = t;
    val += data[t];
  }
  val += some_function(100);

  /*
    Here we reference 'val' so that the above stack data doesn't
    get optimized out, and enter a trivial loop.
  */
  while (val) {
    rt_task_wait_period();
  }

  return;
}

/*
  Initialize the stack with a recognizable pattern, one that will
  be looked for in rt_task_stack_check() to see how much was actually
  used.
 */
int rt_task_stack_init(RT_TASK * task, int pattern)
{
  long int * ptr;

  /*
    Clobber the stack with a recognizable pattern. The bottom,
    task->stack_bottom, is writeable. The top, task->stack,
    points to some RTAI data and can't be written.
  */
  for (ptr = task->stack_bottom; ptr < task->stack; ptr++) {
    *ptr = pattern;
  }

  return 0;
}

/*
  Check the stack and see how much was used by comparing what's left
  with the initialization pattern.
 */
int rt_task_stack_check(RT_TASK * task, int pattern)
{
  long int * ptr;
  int unused;

  /*
    Read up from the bottom and count the unused bytes.
  */
  for (unused = 0, ptr = task->stack_bottom; ptr < task->stack; ptr++) {
    if (*ptr != pattern) {
      break;
    }
    unused += sizeof(int);
  }

  return unused;
}

int init_module(void)
{
  /*
    This stack doesn't count against our task stack.
   */
  RTIME task_period;
  int retval;

  stack_task_inited = 0;
  retval = rt_task_init(&stack_task, task_code, 0,
			STACKSIZE,
			RT_LOWEST_PRIORITY, 0, 0);
  if (retval) {
    printk("could not init task\n");
    return retval;
  }
  stack_task_inited = 1;

  /* initialize the stack with a recognizable pattern */
  rt_task_stack_init(&stack_task, pattern);

  /* run our trivial task in one-shot mode at 1 millisecond intervals */
  rt_set_oneshot_mode();
  start_rt_timer(0);
  task_period = nano2count(1e6);
  retval = rt_task_make_periodic(&stack_task,
				 rt_get_time() + task_period, 
				 task_period);
  if (retval) {
    printk("could not start task\n");
    return 0;
  }

  return 0;
}

void cleanup_module(void)
{
  long int * ptr;

  if (stack_task_inited) {
    rt_task_suspend(&stack_task);

    /*
      Print out stack from top down.
     */
    for (ptr = stack_task.stack-1; ptr >= stack_task.stack_bottom; ptr--) {
      printk("%X: %X\n", (unsigned int) ptr, (unsigned int) *ptr);
    }

    /*
      Print out the size of the unused portion.
     */
    printk("%d unused stack bytes\n", 
	   rt_task_stack_check(&stack_task, pattern));

    rt_task_delete(&stack_task);
    stack_task_inited = 0;
  }

  stop_rt_timer();
  
  return;
}
