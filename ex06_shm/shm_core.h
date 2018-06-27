#ifndef SHM_CORE_H
#define SHM_CORE_H

/*
  shm_common.h

  Declarations for the shared memory data structure, and the functions
  that will be used for data consistency.
 */

#define SHM_HOWMANY 1000	/* the bigger, the more time-consuming */

typedef struct {
  unsigned char head;		/* for the HEAD_TAIL technique */
  unsigned char reader;		/* for the PETERSON technique */
  unsigned char writer;		/* ditto */
  unsigned char favored;	/* ditto */
  int tas;			/* for the TEST_AND_SET technique */
  int data[SHM_HOWMANY];	/* we'll fill this with a heartbeat */
  int made_writes;		/* how many writes the writer made */
  int missed_writes;		/* how many writes the writer missed */
  unsigned char tail;		/* for the HEAD_TAIL technique */
} SHM_STRUCT;

#define SHM_KEY 101		/* shared key, arbitrary value */

/* which algorithm will be used */
enum {PETERSON = 1, TEST_AND_SET = 2, HEAD_TAIL = 3};

/*
  Function pointer declarations. We will declare a reader function and
  a writer function, and set them to be one of the ones listed below.
 */
typedef int (* MUTEX_READ_FUNC)(SHM_STRUCT * src, SHM_STRUCT * dst, int * come_back);
typedef int (* MUTEX_WRITE_FUNC)(SHM_STRUCT * src, SHM_STRUCT * dst, int * come_back);

/*
  Here are the various reader and writer functions for data consistency,
  with obvious names
 */

extern int peterson_read(SHM_STRUCT * shm_ptr, SHM_STRUCT * shm_copy, int * come_back);
extern int peterson_write(SHM_STRUCT * shm_copy, SHM_STRUCT * shm_ptr, int * come_back);

extern int test_and_set_read(SHM_STRUCT * shm_ptr, SHM_STRUCT * shm_copy, int * come_back);
extern int test_and_set_write(SHM_STRUCT * shm_copy, SHM_STRUCT * shm_ptr, int * come_back);

extern int head_tail_read(SHM_STRUCT * shm_ptr, SHM_STRUCT * shm_copy, int * come_back);
extern int head_tail_write(SHM_STRUCT * shm_copy, SHM_STRUCT * shm_ptr, int * come_back);

#endif /* SHM_COMMON_H */
