/*
  shm_task.c

  Set up a periodic task that increments a heartbeat array in shared memory.
  This example demonstrates the allocation, use and freeing of shared memory.

  Several different data consistency algorithms can be selected; see
  shm_common.h and shm_common.c for the implementations. In this code
  we set up a function pointer depending upon which one we want.

  Performance measures for the number of made and missed writes are kept
  in shared memory, along with a heartbeat array, so that the Linux process
  can print them out at the end.

  This example also demonstates how to pass arguments to a kernel module
  via 'insmod variable=<value>'. This is used to specify which algorithm
  to use, head/tail or read/write flags.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/errno.h>	/* ENOMEM */
#include <linux/moduleparam.h>
#include <stddef.h>		/* sizeof() */
#include "rtai.h"
#include "rtai_sched.h"
#include "rtai_shm.h"		/* rtai_kmalloc(), rtai_kfree() */
#include "shm_core.h"		/* SHM_STRUCT, SHM_KEY, SHM_HOWMANY */

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

RT_TASK shm_task;

static RTIME shm_period_ns = 100000;

static SHM_STRUCT * shm_ptr = 0; /* pointer to shared memory */

/*
  To allow arguments to be passed to your module, declare the variables
  that will take the values of the command line arguments as global and
  then use the module_param() macro, (defined in linux/moduleparam.h) to
  set the mechanism up. At runtime, insmod will fill the variables with
  any command line arguments that are given, like ./insmod mymodule.ko
  myvariable=5. The variable declarations and macros should be placed at
  the beginning of the module for clarity. The example code should clear
  up my admittedly lousy explanation.

  The module_param() macro takes 3 arguments: the name of the
  variable, its type and permissions for the corresponding file in
  sysfs. Integer types can be signed as usual or unsigned. If you'd
  like to use arrays of integers or strings see module_param_array()
  and module_param_string().

  int myint = 3;
  module_param(myint, int, 0);

  Arrays are supported too, but things are a bit different now than
  they were in the 2.4. days. To keep track of the number of
  parameters you need to pass a pointer to a count variable as third
  parameter. At your option, you could also ignore the count and pass
  NULL instead. We show both possibilities here:

  int myintarray[2];
  module_param_array(myintarray, int, NULL, 0); not interested in count

  int myshortarray[4];
  int count;
  module_parm_array(myshortarray, short, & count, 0);
  put count into "count" variable

  E.g., 

  static short int myshort = 1;
  static int myint = 420;
  static long int mylong = 9999;
  static char *mystring = "blah";
  static int myintArray[2] = { -1, -1 };
  static int arr_argc = 0;

  module_param(foo, int, 0000)
  The first param is the parameters name
  The second param is it's data type
  The final argument is the permissions bits, 
  for exposing parameters in sysfs (if non-zero) at a later stage.

  module_param(myshort, short, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
  MODULE_PARM_DESC(myshort, "A short integer");
  module_param(myint, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  MODULE_PARM_DESC(myint, "An integer");
  module_param(mylong, long, S_IRUSR);
  MODULE_PARM_DESC(mylong, "A long integer");
  module_param(mystring, charp, 0000);
  MODULE_PARM_DESC(mystring, "A character string");

  module_param_array(name, type, num, perm);
  The first param is the parameter's (in this case the array's) name The
  second param is the data type of the elements of the array The third
  argument is a pointer to the variable that will store the number of
  elements of the array initialized by the user at module loading time
  The fourth argument is the permission bits

  module_param_array(myintArray, int, &arr_argc, 0000);
  MODULE_PARM_DESC(myintArray, "An array of integers");

  Make sure to #include <linux/moduleparam.h>
*/

/*
  In shm_common.c are several different data consistency algorithms,
  with symbolic names declared in shm_common.h, e.g., PETERSON,
  TEST_AND_SET, HEAD_TAIL. Also declared is a function pointer type,
  MUTEX_WRITE_FUNC, for a pointer to one of these functions. We
  set this here so we can switch between algorithms at run time.
 */

int WHICH_ALGO = PETERSON;
module_param(WHICH_ALGO, int, PETERSON);

static MUTEX_WRITE_FUNC algo_ptr = peterson_write;

static int made_writes = 0;
static int missed_writes = 0;

static void shm_writer(int arg)
{
  SHM_STRUCT shm_copy;
  int come_back;		/* persistent flag, needed for some algos */
  int heartbeat;		/* we increment this and fill the data array */
  int t;

  come_back = 0;
  heartbeat = 0;

  /*
    In this writer code, we just reference the shared memory pointer
    as we would any other pointer. Here we increment a heartbeat that
    we write into the datap[] array, update the structure's made_writes
    and missed_writes, then call the function pointer that does the
    data consistency work.
   */
  while (1) {
    heartbeat++;
    for (t = 0; t < SHM_HOWMANY; t++) {
      shm_copy.data[t] = heartbeat;
    }
    shm_copy.made_writes = made_writes;
    shm_copy.missed_writes = missed_writes;

    if (0 == (*algo_ptr)(&shm_copy, shm_ptr, &come_back)) {
      made_writes++;
    } else {
      missed_writes++;
    }

    rt_task_wait_period();
  }

  return;
}

int init_module(void)
{
  int t;
  int retval;
  RTIME shm_period_count;

  /*
    Allocate shared memory by calling

    void * rtai_kmalloc(int key, size_t size);

    which takes an integer 'key' agreed to by all memory sharers, and
    the size of shared memory, and returns a pointer to it.
  */
  shm_ptr = rtai_kmalloc(SHM_KEY, SHM_HOWMANY * sizeof(int));
  if (0 == shm_ptr) {
    return -ENOMEM;		/* can't get memory-- perhaps too big */
  }

  /* initialize shared memory */
  shm_ptr->head = 0;
  shm_ptr->reader = 0;
  shm_ptr->writer = 0;
  shm_ptr->favored = 0;
  shm_ptr->tas = 0;
  for (t = 0; t < SHM_HOWMANY; t++) {
    shm_ptr->data[t] = 0;
  }
  shm_ptr->made_writes = 0;
  shm_ptr->missed_writes = 0;
  shm_ptr->tail = 0;

  /* set up algorithm pointer */
  if (WHICH_ALGO == TEST_AND_SET) {
    algo_ptr = test_and_set_write;
  } else if (WHICH_ALGO == HEAD_TAIL) {
    algo_ptr = head_tail_write;
  } /* else leave it at peterson */
  
  /*
    We'll run our task as usual, a single pure periodic task
   */

  rt_set_periodic_mode();
  shm_period_count = nano2count(shm_period_ns);
  start_rt_timer(shm_period_count);
  retval = rt_task_init(&shm_task, shm_writer, 0, 
			1024 + sizeof(SHM_STRUCT),
			RT_LOWEST_PRIORITY, 0, 0);
  if (0 != retval) {
    return retval;
  }

  retval = rt_task_make_periodic(&shm_task,
				 rt_get_time() + shm_period_count,
				 shm_period_count);
  if (0 != retval) {
    return retval;
  }

  return 0;
}

void cleanup_module(void)
{
  rt_task_delete(&shm_task);

  /*
    Free up the shared memory by calling

    void rtai_kfree(int key);

    using the same agreed-to key as we used to allocate it.
  */
  rtai_kfree(SHM_KEY);

  return;
}
