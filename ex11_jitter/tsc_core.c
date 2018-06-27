/*
  tsc_core.c

  Time stamp counter (TSC) utilities for reading the Pentium TSC,
  computing differences and calibrating the TSC-to-seconds factor.
*/

/* workaround for limits.h not being found for kernel compiles */
#ifdef __KERNEL__
#define ULONG_MAX 4294967295UL
#else
#include <sys/time.h>		/* gettimeofday(), struct timeval */
#include <time.h>		/* nanosleep(), struct timespec */
#include <limits.h>		/* ULONG_MAX */
#endif

#include "tsc.h"

/*
  get_tsc() stores the Pentium time stamp counter, a 64-bit integer that
  increments each CPU clock cycle, into the TSC structure
*/
void get_tsc(TSC * tsc)
{
  unsigned long int eax, edx;

  __asm__ __volatile__("rdtsc":"=a"(eax), "=d"(edx));

  tsc->big = edx;
  tsc->small = eax;

  return;
}

/*
  diff_tsc() effectively puts (first - second) into tsc, so you can get the
  difference between two time stamp counts
*/
void diff_tsc(TSC first, TSC second, TSC * tsc)
{
  if (tsc_isgreaterequal(first, second)) {
    if (first.small >= second.small) {
      tsc->small = first.small - second.small;
      tsc->big = first.big - second.big;
    } else {
      tsc->small = ULONG_MAX - (second.small - first.small) + 1;
      tsc->big = first.big - second.big - 1;
    }
  } else {
    if (second.small <= first.small) {
      tsc->small = first.small - second.small;
      tsc->big = ULONG_MAX - (second.big - first.big) + 1;
    } else {
      tsc->small = ULONG_MAX - (second.small - first.small) + 1;
      tsc->big = ULONG_MAX - (second.big - first.big);
    }
  }
}

/*
  Convert a time stamp count into a double, with the same units but more
  easily manipulated in calculations, e.g, averaging.
*/
double tsc_to_double(TSC tsc)
{
  return ((double) tsc.big) * ((double) ULONG_MAX) + (double) tsc.small;
}

/*
  Convert a double into a time stamp count, with same units but as the
  TSC structure. 'd' is assumed to be non-negative.
*/
TSC double_to_tsc(double d)
{
  TSC tsc;

  tsc.big = (unsigned long int) (d / ((double) ULONG_MAX));
  tsc.small = (unsigned long int) (d - ((double) tsc.big) * ((double) ULONG_MAX));
  
  return tsc;
}

#ifndef __KERNEL__

/*
  Compile this for non-kernel code only, since we call gettimeofday()
*/
double calibrate_cpu_secs_per_cycle(void)
{
  TSC then, now;
  struct timeval tv_start, tv_end;
  struct timespec ts;
  double tsc_start, tsc_end;
  double delta_t;

  /* get an accurate estimate of wall time in cpu counts by
     bracketing wall time measurement from gettimeofday() with
     calls to TSC, then average them */
  get_tsc(&then);
  gettimeofday(&tv_start, NULL);
  get_tsc(&now);
  tsc_start = 0.5 * (tsc_to_double(now) + tsc_to_double(then));

  /* sleep about 1 second, actual time not important */
  ts.tv_sec = 1;
  ts.tv_nsec = 0;
  nanosleep(&ts, NULL);

  /* get another accurate estimate of wall time in cpu counts */
  get_tsc(&then);
  gettimeofday(&tv_end, NULL);
  get_tsc(&now);
  tsc_end = 0.5 * (tsc_to_double(now) + tsc_to_double(then));

  /* get the elapsed wall time by "subtracting" secs-microsecs structures */
  if (tv_end.tv_usec < tv_start.tv_usec) {
    delta_t = 1.0 - ((double) (tv_start.tv_usec - tv_end.tv_usec)) / 1.0e6;
    delta_t += (double) (tv_end.tv_sec - tv_start.tv_sec - 1);
  } else {
    delta_t = ((double) (tv_end.tv_usec - tv_start.tv_usec)) / 1.0e6;
    delta_t += (double) (tv_end.tv_sec - tv_start.tv_sec);
  }

  /* seconds per cycle is elapsed wall time in nanoseconds divided
     by elapsed TSC time */
  return delta_t / (tsc_end - tsc_start);
}

#endif
