/* Restartable Sequences internal API.  Linux implementation.
   Copyright (C) 2021-2023 Free Software Foundation, Inc.

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

#ifndef RSEQ_INTERNAL_H
#define RSEQ_INTERNAL_H

#include <sysdep.h>
#include <errno.h>
#include <kernel-features.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/rseq.h>
#include <ldsodefs.h>

/* rseq area registered with the kernel.  Use a custom definition
   here to isolate from kernel struct rseq changes.  The
   implementation of sched_getcpu needs acccess to the cpu_id field;
   the other fields are unused and not included here.  */
struct rseq_area
{
  uint32_t cpu_id_start;
  uint32_t cpu_id;
};

static inline struct rseq_area *
rseq_get_area(struct pthread *self)
{
  return (struct rseq_area *) ((char *) self + GLRO (dl_tls_rseq_offset));
}

#ifdef RSEQ_SIG
static inline bool
rseq_register_current_thread (struct pthread *self, bool do_rseq)
{
  if (do_rseq)
    {
      int ret = INTERNAL_SYSCALL_CALL (rseq, rseq_get_area(self),
                                       GLRO (dl_tls_rseq_size),
                                       0, RSEQ_SIG);
      if (!INTERNAL_SYSCALL_ERROR_P (ret))
        return true;
    }
  RSEQ_SETMEM (rseq_get_area(self), cpu_id, RSEQ_CPU_ID_REGISTRATION_FAILED);
  return false;
}
#else /* RSEQ_SIG */
static inline bool
rseq_register_current_thread (struct pthread *self, bool do_rseq)
{
  RSEQ_SETMEM (rseq_get_area(self), cpu_id, RSEQ_CPU_ID_REGISTRATION_FAILED);
  return false;
}
#endif /* RSEQ_SIG */

#endif /* rseq-internal.h */
