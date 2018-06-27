#ifndef COMMON_H
#define COMMON_H

/*
  Those who share the memory must agree to a unique key to identify it
  among the pool of shared memory used by everyone else. Pick an used
  number, using the Linux command "ipcs" (interprocess communication status)
  to see what's already in use.
 */
enum {SHM_KEY = 101};

/*
  How many data elements are to be allocated for our array of TSC structs.
  The actual space we will allocate will be SHM_HOWMANY * sizeof(TSC).
 */
enum {SHM_HOWMANY = 1024};

/*
  The nominal period for the jitter task, in nanoseconds
 */
enum {PERIOD_NSEC = 50000};	/* 50 microseconds */

#endif /* COMMON_H */
