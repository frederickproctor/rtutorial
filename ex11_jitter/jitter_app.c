/*
  jitter_app.c

  Reads shared memory for a log of time stamp counts, differences them
  and converts to microseconds, and dumps them to a file for later plotting.
 */

/*
  THIS SOFTWARE WAS PRODUCED BY EMPLOYEES OF THE U.S. GOVERNMENT AS PART
  OF THEIR OFFICIAL DUTIES AND IS IN THE PUBLIC DOMAIN.
*/

#include <stdio.h>		/* printf() */
#include <stddef.h>		/* sizeof() */
#include <sys/mman.h>		/* PROT_READ, needed for rtai_shm.h */
#include <sys/types.h>		/* off_t, needed for rtai_shm.h */
#include <sys/fcntl.h>		/* O_RDWR, needed for rtai_shm.h */
#include <rtai_shm.h>		/* rtai_malloc,free() */
#include "common.h"		/* SHM_KEY, SHM_HOWMANY */
#include "tsc.h"		/* TSC structure, diff_tsc(), calibrate... */

int main(void)
{
  double cpu_microsecs_per_cycle;
  double delta;
  TSC * tsc_array;
  TSC this_tsc, zero_tsc = {0, 0};
  int t;

  cpu_microsecs_per_cycle = calibrate_cpu_secs_per_cycle() * 1.0e6;

  tsc_array = rtai_malloc(SHM_KEY, SHM_HOWMANY * sizeof(TSC));
  if (0 == tsc_array) {
    fprintf(stderr, "can't allocate shared memory\n");
    return 1;
  }

  /*
    Wait for TSC log to fill up by looking at the last one, which
    was initially set to zero by the RT task and will be the last
    one written. It's possible that the last TSC could be legitimately
    zero, in which case the loop will never end. The odds of this are
    one in 2^64, or one in 18,446,744,073,709,551,616.
  */
  do {
    this_tsc = tsc_array[SHM_HOWMANY - 1];
  } while (tsc_isequal(this_tsc, zero_tsc));

  /*
    Difference the time stamp values, convert to seconds and print them out.
   */
  for (t = 1; t < SHM_HOWMANY; t++) {
    diff_tsc(tsc_array[t], tsc_array[t - 1], &this_tsc);
    delta = tsc_to_double(this_tsc);
    delta *= cpu_microsecs_per_cycle;
    printf("%f\n", delta);
  }

  rtai_free(SHM_KEY, tsc_array);

  return 0;
}
