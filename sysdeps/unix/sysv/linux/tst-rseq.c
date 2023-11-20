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
#include <stdio.h>
#include <sys/rseq.h>
#include <unistd.h>

#ifdef RSEQ_SIG
# include <errno.h>
# include <error.h>
# include <stdlib.h>
# include <string.h>
# include <syscall.h>
# include <thread_pointer.h>
# include <tls.h>
# include <sys/auxv.h>
# include "tst-rseq.h"

static void
do_rseq_main_test (void)
{
  size_t rseq_align = MAX (getauxval (AT_RSEQ_ALIGN), RSEQ_TEST_MIN_ALIGN);
  size_t rseq_size = roundup (MAX (getauxval (AT_RSEQ_FEATURE_SIZE), RSEQ_TEST_MIN_SIZE), rseq_align);
  struct rseq *rseq = __thread_pointer () + __rseq_offset;

  TEST_VERIFY_EXIT (rseq_thread_registered ());
  TEST_COMPARE (__rseq_flags, 0);
  TEST_COMPARE (__rseq_size, rseq_size);
  /* The size of the rseq area must be a multiple of the alignment.  */
  TEST_VERIFY ((__rseq_size % rseq_align) == 0);
  /* The rseq area address must be aligned.  */
  TEST_VERIFY (((unsigned long) rseq % rseq_align) == 0);
#if TLS_TCB_AT_TP
  /* The rseq area block should come before the thread pointer and be at least 32 bytes. */
  TEST_VERIFY (__rseq_offset <= RSEQ_TEST_MIN_SIZE);
#elif TLS_DTV_AT_TP
  /* The rseq area block should come after the thread pointer. */
  TEST_VERIFY (__rseq_offset >= 0);
#else
# error "Either TLS_TCB_AT_TP or TLS_DTV_AT_TP must be defined"
#endif
}

static void
do_rseq_test (void)
{
  if (!rseq_available ())
    {
      FAIL_UNSUPPORTED ("kernel does not support rseq, skipping test");
    }
  do_rseq_main_test ();
}
#else /* RSEQ_SIG */
static void
do_rseq_test (void)
{
  FAIL_UNSUPPORTED ("glibc does not define RSEQ_SIG, skipping test");
}
#endif /* RSEQ_SIG */

static int
do_test (void)
{
  do_rseq_test ();
  return 0;
}

#include <support/test-driver.c>
