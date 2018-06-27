/*
  shm_core.c

  Various mutual exclusion techniques for reader/writer pairs. These
  functions take a shared memory pointer to our data structure, and
  a local copy, and either reads out shared memory into the copy or
  writes the copy into shared memory. The functions return 0 if this
  was achieved, 1 if it could not be due to the shared memory being used
  by another process.
*/

#include "shm_core.h"		/* SHM_STRUCT */

/*
  Peterson's algorithm, an improvement over the venerable Dekker's algorithm,
  used for two-process mutual exclusion. See H.M. Deitel, "An Introduction
  to Operating Systems," Second Edition, pp. 85-86, Addison-Wesley, 1990.

  This algorithm has been changed slightly so that it doesn't enter a tight
  spinlock loop inside this function. We break out if we can't get it, and
  re-enter again the next time it's called, using the 'come_back' flag
  provided to store this state.
*/

int peterson_read(SHM_STRUCT * shm_ptr, SHM_STRUCT * shm_copy, int * come_back)
{
  int t;

  if (! *come_back) {
    shm_ptr->reader = 1;
    shm_ptr->favored = 1;	/* writer */
  }

  if (shm_ptr->writer && shm_ptr->favored == 1) {
    /* we didn't get it */
    *come_back = 1;
    return 1;
  }

  /* else we got it, so copy data from shm into our copy */
  for (t = 0; t < SHM_HOWMANY; t++) {
    shm_copy->data[t] = shm_ptr->data[t];
  }
  shm_copy->made_writes = shm_ptr->made_writes;
  shm_copy->missed_writes = shm_ptr->missed_writes;

  /* give it up */
  shm_ptr->reader = 0;
  *come_back = 0;

  return 0;
}

int peterson_write(SHM_STRUCT * shm_copy, SHM_STRUCT * shm_ptr, int * come_back)
{
  int t;

  if (! *come_back) {
    shm_ptr->writer = 1;
    shm_ptr->favored = 0;	/* reader */
  }

  if (shm_ptr->reader && shm_ptr->favored == 0) {
    /* we didn't get it */
    *come_back = 1;
    return 1;
  }

  /* else we got it, so copy data from our copy to shm */
  for (t = 0; t < SHM_HOWMANY; t++) {
    shm_ptr->data[t] = shm_copy->data[t];
  }
  shm_ptr->made_writes = shm_copy->made_writes;
  shm_ptr->missed_writes = shm_copy->missed_writes;

  /* give it up */
  shm_ptr->writer = 0;
  *come_back = 0;

  return 0;
}

/*
  Here we insert some assembly language to get the Intel XCHG command,
  following Gnu C's strange method. For the details, see

  http://www.ibiblio.org/gferg/ldp/GCC-Inline-Assembly-HOWTO.html
  http://www-106.ibm.com/developerworks/linux/library/l-ia.html
  http://www.linux.org/docs/ldp/howto/Assembly-HOWTO/gcc.html
 */

static inline unsigned int test_and_set(volatile unsigned int *addr)
{
  int oldval;

  __asm__ __volatile__("xchgl %0, %1"
		       : "=r"(oldval), "=m"(*(addr))
		       : "0"(1), "m"(*(addr))
		       : "memory");

  return oldval;
}

static inline unsigned int test_and_clear(volatile unsigned int *addr)
{
  int oldval;

  __asm__ __volatile__("xchgl %0, %1"
		       : "=r"(oldval), "=m"(*(addr))
		       : "0"(0), "m"(*(addr))
		       : "memory");

  return oldval;
}

/*
  The Test and Set technique does an atomic read and then write of a flag.
  To try and acquire the flag, test and set exchanges 1 with the flag
  atomically (without the possibility of interruption), and returns what
  was in the flag. If it was 0, then the resource was free, the flag is
  now 1, and you have it. If it was 1, then the resource was in use, the
  flag is still 1, and you don't have it.
 */

int test_and_set_read(SHM_STRUCT * shm_ptr, SHM_STRUCT * shm_copy, int * come_back)
{
  int t;

  if (0 != test_and_set((volatile unsigned int *) &shm_ptr->tas)) {
    /* we missed it */
    return 1;
  }

  /* else we got it */
  for (t = 0; t < SHM_HOWMANY; t++) {
    shm_copy->data[t] = shm_ptr->data[t];
  }
  shm_copy->made_writes = shm_ptr->made_writes;
  shm_copy->missed_writes = shm_ptr->missed_writes;

  /* give it up */
  test_and_clear((volatile unsigned int *) &shm_ptr->tas); /* give it up */

  return 0;
}

int test_and_set_write(SHM_STRUCT * shm_copy, SHM_STRUCT * shm_ptr, int * come_back)
{
  int t;

  /*
    Strictly speaking, we don't need to do a test-and-set since the RT writer
    can't be interrupted by the Linux reader. We can simply test it, then
    set it, since this group will be atomic. For generality, we'll do the
    test and set anyway.
   */
  if (0 != test_and_set((volatile unsigned int *) &shm_ptr->tas)) {
    /* we missed it */
    return 1;
  }

  /* else we got it */
  for (t = 0; t < SHM_HOWMANY; t++) {
    shm_ptr->data[t] = shm_copy->data[t];
  }
  shm_ptr->made_writes = shm_copy->made_writes;
  shm_ptr->missed_writes = shm_copy->missed_writes;

  /* give it up */
  test_and_clear((volatile unsigned int *) &shm_ptr->tas); /* give it up */
  
  return 0;
}

/*
  With head/tail flags, the writer increments a head count, writes the
  data, and sets the tail count to the head count. This always succeeds.
  After reading, the reader checks the head and tail. If they are the same,
  the read went OK and 0 is returned. If they are not, the read was split
  by the write and 1 is returned.

  This technique only works if one of the processes can't be interrupted
  by the other. In RT Linux, this is true if one is an RT task and the other
  is a Linux process.
 */

int head_tail_read(SHM_STRUCT * shm_ptr, SHM_STRUCT * shm_copy, int * come_back)
{
  int t;

  shm_copy->head = shm_ptr->head;
  for (t = 0; t < SHM_HOWMANY; t++) {
    shm_copy->data[t] = shm_ptr->data[t];
  }
  shm_copy->made_writes = shm_ptr->made_writes;
  shm_copy->missed_writes = shm_ptr->missed_writes;
  shm_copy->tail = shm_ptr->tail;

  if (shm_copy->head != shm_copy->tail) {
    return 1;
  }

  return 0;
}

int head_tail_write(SHM_STRUCT * shm_copy, SHM_STRUCT * shm_ptr, int * come_back)
{
  int t;

  shm_ptr->head++;
  for (t = 0; t < SHM_HOWMANY; t++) {
    shm_ptr->data[t] = shm_copy->data[t];
  }
  shm_ptr->made_writes = shm_copy->made_writes;
  shm_ptr->missed_writes = shm_copy->missed_writes;
  shm_ptr->tail = shm_ptr->head;

  return 0;
}
