/* Restartable Sequences single-threaded tests.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

/* These tests validate that rseq is registered from main in an executable
   not linked against libpthread.  */

#include <support/check.h>
#include <support/namespace.h>
#include <support/xthread.h>
#include <stdio.h>
#include <sys/rseq.h>
#include <unistd.h>

#ifdef RSEQ_SIG
# include <errno.h>
# include <error.h>
# include <stdlib.h>
# include <string.h>
# include <syscall.h>
# include <sys/auxv.h>
# include <thread_pointer.h>
# include <tls.h>
# include <sys/auxv.h>
# include "tst-rseq.h"

static void
do_rseq_main_test (void)
{
  size_t rseq_align = MAX (getauxval (AT_RSEQ_ALIGN), RSEQ_TEST_MIN_ALIGN);
  size_t rseq_feature_size = MAX (getauxval (AT_RSEQ_FEATURE_SIZE), RSEQ_TEST_MIN_FEATURE_SIZE);
  size_t rseq_alloc_size = roundup (MAX (rseq_feature_size, RSEQ_TEST_MIN_SIZE), rseq_align);
  struct rseq *global_rseq = __thread_pointer () + __rseq_offset;

  TEST_VERIFY_EXIT (rseq_thread_registered ());

  /* __rseq_flags is unused and should always be '0'.  */
  TEST_COMPARE (__rseq_flags, 0);

  /* When rseq is registered, __rseq_size should report the feature size.  */
  TEST_COMPARE (__rseq_size, rseq_feature_size);

  /* When rseq is registered, the 'cpu_id' field should be set to a valid cpu
   * number.  */
  TEST_VERIFY ((int32_t) global_rseq->cpu_id >= 0);

  /* The rseq area address must be aligned.  */
  TEST_VERIFY (((unsigned long) global_rseq % rseq_align) == 0);

#if TLS_TCB_AT_TP
  /* The rseq area block should come before the thread pointer and be at least 32 bytes. */
  TEST_VERIFY (__rseq_offset <= RSEQ_TEST_MIN_SIZE);
#elif TLS_DTV_AT_TP
  /* The rseq area block should come after the thread pointer. */
  TEST_VERIFY (__rseq_offset >= 0);
#else
# error "Either TLS_TCB_AT_TP or TLS_DTV_AT_TP must be defined"
#endif

  /* Test a rseq registration with the same arguments as the internal
     registration which should fail with errno == EBUSY.  */
  TEST_VERIFY (((unsigned long) global_rseq % rseq_align) == 0);
  TEST_VERIFY (__rseq_size <= rseq_alloc_size);
  int ret = syscall (__NR_rseq, global_rseq, rseq_alloc_size, 0, RSEQ_SIG);
  TEST_VERIFY (ret != 0);
  TEST_COMPARE (errno, EBUSY);
}

static void *
thread_func (void *ignored)
{
  do_rseq_main_test ();
  return NULL;
}

static void
proc_func (void *ignored)
{
  do_rseq_main_test ();
}

static int
do_test (void)
{
  if (!rseq_available ())
    {
      FAIL_UNSUPPORTED ("kernel does not support rseq, skipping test");
    }
  printf ("info: __rseq_size: %u\n", __rseq_size);
  printf ("info: __rseq_offset: %td\n", __rseq_offset);
  printf ("info: __rseq_flags: %u\n", __rseq_flags);
  printf ("info: getauxval (AT_RSEQ_FEATURE_SIZE): %ld\n",
          getauxval (AT_RSEQ_FEATURE_SIZE));
  printf ("info: getauxval (AT_RSEQ_ALIGN): %ld\n", getauxval (AT_RSEQ_ALIGN));

  puts ("info: checking main thread");
  do_rseq_main_test ();

  puts ("info: checking main thread (2)");
  do_rseq_main_test ();

  puts ("info: checking new thread");
  xpthread_join (xpthread_create (NULL, thread_func, NULL));

  puts ("info: checking subprocess");
  support_isolate_in_subprocess (proc_func, NULL);

  return 0;
}
#else /* RSEQ_SIG */
static int
do_test (void)
{
  FAIL_UNSUPPORTED ("glibc does not define RSEQ_SIG, skipping test");
}
#endif /* RSEQ_SIG */

#include <support/test-driver.c>
