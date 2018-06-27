#ifndef TSC_H
#define TSC_H

typedef struct {
  unsigned long int big;
  unsigned long int small;
} TSC;

#define tsc_isequal(t1,t2) (((t1).small == (t2).small) && ((t1).big == (t2).big))

#define tsc_isgreaterequal(t1,t2) ((t1).big > (t2).big ? 1 : (t1).big < (t2).big ? 0 : (t1).small >= (t2).small ? 1 : 0)

/*
  get_tsc() stores the Pentium time stamp counter, a 64-bit integer that
  increments each CPU clock cycle, into the TSC structure
*/
extern void get_tsc(TSC * tsc);

/*
  diff_tsc() effectively puts (first - second) into tsc, so you can get the
  difference between two time stamp counts.
*/
extern void diff_tsc(TSC first, TSC second, TSC * diff);

/*
  Convert a time stamp count into a double, with the same units but more
  easily manipulated in calculations, e.g, averaging.
*/
extern double tsc_to_double(TSC tsc);

/*
  Convert a double into a time stamp count, with same units but as the
  TSC structure. 'd' is assumed to be non-negative.
*/
extern TSC double_to_tsc(double d);

/*
  calibrate_cpu_secs_per_cycle() takes two readings of the TSC and the
  wall clock time, and computes the real time in seconds for a CPU
  clock tick.

  The "seconds per cycle" return makes this a multiplier to convert
  time stamps to seconds.
*/
extern double calibrate_cpu_secs_per_cycle(void);

#endif /* TSC_H */

