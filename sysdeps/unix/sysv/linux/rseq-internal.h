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

/* rseq area registered with the kernel.  Use a custom definition here to
   isolate from the system provided header which could lack some fields of the
   Extended ABI.

   Access to fields of the Extended ABI beyond the 20 bytes of the original ABI
   (after 'flags') must be gated by a check of the feature size.  */
struct rseq_area
{
  /* Original ABI.  */
  uint32_t cpu_id_start;
  uint32_t cpu_id;
  uint64_t rseq_cs;
  uint32_t flags;
  /* Extended ABI.  */
  uint32_t node_id;
  uint32_t mm_cid;
};

/* Minimum size of the rseq area.  */
#define RSEQ_AREA_MIN_SIZE 32

/* Minimum feature size of the rseq area.  */
#define RSEQ_MIN_FEATURE_SIZE 20

/* Minimum alignment of the rseq area.  */
#define RSEQ_MIN_ALIGN 32

/* Size of the active features in the rseq area of the current registration, 0
   if registration failed.
   In .data.relro but not yet write-protected.  */
extern unsigned int _rseq_size attribute_hidden;

/* Offset from the thread pointer to the rseq area, always set to allow
   checking the registration status by reading the 'cpu_id' field.
   In .data.relro but not yet write-protected.  */
extern ptrdiff_t _rseq_offset attribute_hidden;

/* Returns a pointer to the current thread rseq area.  */
static inline struct rseq_area *
rseq_get_area(void)
{
#if IS_IN (rtld)
  /* Use the hidden symbol in ld.so.  */
  return (struct rseq_area *) ((char *) __thread_pointer() + _rseq_offset);
#else
  return (struct rseq_area *) ((char *) __thread_pointer() + __rseq_offset);
#endif
}

#ifdef RSEQ_SIG
static inline bool
rseq_register_current_thread (struct pthread *self, bool do_rseq)
{
  if (do_rseq)
    {
      unsigned int size;

      /* Get the feature size from the auxiliary vector, this will always be at
         least 20 bytes.  */
      size = GLRO (dl_rseq_feature_size);

      /* The feature size can be smaller than the minimum rseq area size of 32
         bytes that the syscall will accept, if this is the case, bump the size
         to the minimum of 32 bytes. */
      if (size < RSEQ_AREA_MIN_SIZE)
        size = RSEQ_AREA_MIN_SIZE;

      /* The kernel expects 'rseq_area->rseq_cs == NULL' on registration, zero
         the whole rseq area.  */
      memset(rseq_get_area(), 0, size);

      int ret = INTERNAL_SYSCALL_CALL (rseq, rseq_get_area(), size, 0,
		      RSEQ_SIG);
      if (!INTERNAL_SYSCALL_ERROR_P (ret))
        return true;
    }

  /* If the registration failed or is disabled by tunable, we have to set 'cpu_id' to
     RSEQ_CPU_ID_REGISTRATION_FAILED to allow userspace to properly test the
     status of the registration.  */
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
