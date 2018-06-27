/*
  shm_app.c

  Connect to real-time shared memory and continually loop, reading out
  the contents and checking for consistent data. We can't use
  semaphores to ensure mutual exclusion, since there are no semaphores
  between Linux processes and real-time tasks. The best we can do is
  use a few techniques to detect inconsistent data, and ignore it when
  that happens.

  Two data integrity techniques are used: head/tail flags, in which
  the real-time task, the writer, will update the shared memory
  structure each cycle without regard to the reader, and bracket the
  data with matching head/tail counts. The reader here reads the
  shared memory, and if the counts are the same, the data is good,
  otherwise its read was split and the data should be discarded.

  The second data integrity technique uses read/write flags. Here the
  writer sets the "I want to write" flag, then checks the "They want
  to read" flag. If the read flag is set, the write is skipped,
  otherwise it proceeds. On the read side, the reader sets the "I want
  to read" flag, then checks the "they want to write" flag. If the
  write flag is set, the read is skipped, otherwise it proceeds.

  Performance measures for the number of made and missed writes are
  kept in shared memory, along with a heartbeat array, so that the
  Linux process can print them out at the end.

  If you pass an argument, e.g., './shm_app ARG', read/write flags
  will be used, otherwise head/tail flags will be used.

  Normally this process runs with fast polling, no sleeps. With
  read/write flags, this can starve the writer: it will rarely see a
  time when we're not asserting that we want to read, and will rarely
  get a chance to write. Since it runs with a sleep (a short one, but
  a sleep nonetheless), it's at a disadvantage. If you pass a second
  argument, e.g., './shm_app ARG ARG', the shortest possible sleep
  will be done, 10 milliseconds typically. This gives the writer a
  chance.

  Performance numbers are printed out at the end, depending upon which
  method is selected, e.g.,

  (running using head/tail flags)
  reads made/missed/inconsistent: 915997/101048/0
  writes made/missed:             101376/0

  (running using read/write flags)
  reads made/missed/inconsistent: 1030569/0/0
  writes made/missed:             43/100330

  (running using read/write flags and sleeps)
  reads made/missed/inconsistent: 1002/0/0
  writes made/missed:             100370/0

  With head/tail flags, fast read polling, we see about a 90% read rate
  with the writer never missing a write.

  With read/write flags, fast read polling, we see a 100% read rate
  with the writer virtually shut out. The few made writes were probably
  done before the reader started up.

  With read/write flags, 10 millisecond read interfvals, we see a
  100% read rate, but with many fewer total reads. The write rate is
  also 100% successful.

  Here, head/tail flags appear to be superior. Even with fast polling,
  90% of the reads are successful, there are many of them (most yielding
  repeated data), and the writer is not penalized.
*/

/*
  THIS SOFTWARE WAS PRODUCED BY EMPLOYEES OF THE U.S. GOVERNMENT AS PART
  OF THEIR OFFICIAL DUTIES AND IS IN THE PUBLIC DOMAIN.
*/

#include <stdio.h>		/* printf() */
#include <stddef.h>		/* sizeof() */
#include <signal.h>		/* signal(), SIGINT */
#include <sys/mman.h>		/* PROT_READ, needed for rtai_shm.h */
#include <sys/types.h>		/* off_t, needed for rtai_shm.h */
#include <sys/fcntl.h>		/* O_RDWR, needed for rtai_shm.h */
#include <rtai_shm.h>		/* rtai_malloc,free() */
#include "shm_core.h"		/* SHM_KEY, SHM_HOWMANY */

/*
  This signal handler just sets the 'done' flag, which we will loop
  on when printing the shared memory. This lets us quit nicely.
 */
static int done = 0;
static void quit(int sig)
{
  done = 1;
}

int main(int argc, char *argv[])
{
  int t;
  SHM_STRUCT * shm_ptr;
  SHM_STRUCT shm_copy;
  int come_back;		/* persistent flag, for some algos */
  int made_reads;		/* how many successful reads we made */
  int missed_reads;		/* how many reads we missed */
  int inconsistent;		/* how many good reads really had bad data */
  int made_writes;		/* copy of last valid made writes, from shm */
  int missed_writes;		/* copy of last valid missed writes */
  MUTEX_READ_FUNC algo_ptr = peterson_read;

  shm_ptr = rtai_malloc(SHM_KEY, SHM_HOWMANY * sizeof(int));
  if (0 == shm_ptr) {
    fprintf(stderr, "can't allocate shared memory\n");
    return 1;
  }

  /*
    Attach our signal hander to SIGINT, the signal raised when we hit
    Control-C.
   */
  signal(SIGINT, quit);

  if (argc > 2) {
    algo_ptr = head_tail_read;
  } else if (argc > 1) {
    algo_ptr = test_and_set_read;
  }

  come_back = 0;
  made_reads = 0;
  missed_reads = 0;
  inconsistent = 0;
  made_writes = 0;
  missed_writes = 0;

  while (! done) {
    if (0 == (*algo_ptr)(shm_ptr, &shm_copy, &come_back)) {
      made_reads++;
      /* check for consistent data */
      for (t = 0; t < SHM_HOWMANY - 1; t++) {
	if (shm_copy.data[t] != shm_copy.data[t + 1]) {
	  made_reads--;
	  inconsistent++;
	  break;
	}
      }
      /* else it's consistent */
      made_writes = shm_copy.made_writes;
      missed_writes = shm_copy.missed_writes;
    } else {
      missed_reads++;
    }
  }

  /*
    Control-C stopped us, so let's print the performance numbers
    and exit nicely by freeing up our connection to shared memory.
   */

  printf("reads made/missed/inconsistent: %d/%d/%d\n",
	 made_reads, missed_reads, inconsistent);
  printf("writes made/missed:             %d/%d\n",
	 made_writes, missed_writes);

  rtai_free(SHM_KEY, shm_ptr);

  return 0;
}
