/* Restartable Sequences internal API.  Linux implementation.
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

#ifndef RSEQ_INTERNAL_H
#define RSEQ_INTERNAL_H

#include <sysdep.h>
#include <errno.h>
#include <kernel-features.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/rseq.h>
#include <thread_pointer.h>
#include <ldsodefs.h>

/* rseq area registered with the kernel.  Use a custom definition
   here to isolate from kernel struct rseq changes.  Access to fields
   beyond the 20 bytes of the original ABI (after 'flags') must be gated
   by a check of the feature size.  */
struct rseq_area
{
  uint32_t cpu_id_start;
  uint32_t cpu_id;
  uint64_t rseq_cs;
  uint32_t flags;
  uint32_t node_id;
  uint32_t mm_cid;
};

static inline struct rseq_area *
rseq_get_area(void)
{
  return (struct rseq_area *) ((char *) __thread_pointer() + GLRO (dl_tls_rseq_offset));
}

#ifdef RSEQ_SIG
static inline bool
rseq_register_current_thread (struct pthread *self, bool do_rseq)
{
  if (do_rseq)
    {
      /* The kernel expects 'rseq_area->rseq_cs == NULL' on registration, zero
         the whole rseq area.  */
      memset(rseq_get_area(), 0, GLRO (dl_tls_rseq_alloc_size));
      int ret = INTERNAL_SYSCALL_CALL (rseq, rseq_get_area(),
                                       GLRO (dl_tls_rseq_alloc_size),
                                       0, RSEQ_SIG);
      if (!INTERNAL_SYSCALL_ERROR_P (ret))
        return true;
    }
  RSEQ_SETMEM (rseq_get_area(), cpu_id, RSEQ_CPU_ID_REGISTRATION_FAILED);
  return false;
}
#else /* RSEQ_SIG */
static inline bool
rseq_register_current_thread (struct pthread *self, bool do_rseq)
{
  RSEQ_SETMEM (rseq_get_area(), cpu_id, RSEQ_CPU_ID_REGISTRATION_FAILED);
  return false;
}
#endif /* RSEQ_SIG */

#endif /* rseq-internal.h */
