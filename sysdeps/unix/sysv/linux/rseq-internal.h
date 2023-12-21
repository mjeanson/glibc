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
#include <thread_pointer.h>
#include <ldsodefs.h>

/* rseq area registered with the kernel.  Use a custom definition
   here to isolate from kernel struct rseq changes.  It must fit in the 32
   bytes of the original ABI and access to fields after 'flags' must be gated
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

/*
 * gcc prior to 4.8.2 miscompiles asm goto.
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=58670
 *
 * gcc prior to 8.1.0 miscompiles asm goto at O1.
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=103908
 *
 * clang prior to version 13.0.1 miscompiles asm goto at O2.
 * https://github.com/llvm/llvm-project/issues/52735
 *
 * Work around these issues by adding a volatile inline asm with
 * memory clobber in the fallthrough after the asm goto and at each
 * label target.  Emit this for all compilers in case other similar
 * issues are found in the future.
 */
#define rseq_after_asm_goto()	__asm__ __volatile__ ("" : : : "memory")

#define __rseq_str_1(x)	#x
#define __rseq_str(x)		__rseq_str_1(x)

/* Offset of rseq_cs field in struct rseq. */
#define RSEQ_CS_OFFSET		8

#define rseq_sizeof_field(TYPE, MEMBER) sizeof((((TYPE *)0)->MEMBER))
#define rseq_offsetofend(TYPE, MEMBER) \
        (offsetof(TYPE, MEMBER) + rseq_sizeof_field(TYPE, MEMBER))

/* Returns a pointer to the current thread rseq area.  */
static inline struct rseq_area *
rseq_get_area(void)
{
  return (struct rseq_area *) ((char *) __thread_pointer() + GLRO (dl_tls_rseq_offset));
}

/* Returns the int value of 'rseq_area->cpu_id'.  */
static inline int
rseq_get_cpu_id(void)
{
  return (int) RSEQ_GETMEM_VOLATILE (rseq_get_area(), cpu_id);
}

/* Returns true if the rseq registration is active.  */
static inline bool
rseq_is_registered(void)
{
  return rseq_get_cpu_id() >= 0;
}

/* Returns true if the current rseq registration has the 'node_id' field.  */
static inline bool
rseq_node_id_available(void)
{
  return GLRO (dl_tls_rseq_feature_size) >= rseq_offsetofend(struct rseq_area, node_id);
}

/* Returns true if the current rseq registration has the 'node_id' field.  */
static inline bool
rseq_mm_cid_available(void)
{
  return GLRO (dl_tls_rseq_feature_size) >= rseq_offsetofend(struct rseq_area, mm_cid);
}

#ifdef RSEQ_SIG
static inline bool
rseq_register_current_thread (struct pthread *self, bool do_rseq)
{
  if (do_rseq)
    {
      /* The kernel expects 'rseq_area->rseq_cs == NULL' on registration, zero
         the whole rseq area.  */
      memset(rseq_get_area(), 0, GLRO (dl_tls_rseq_size));
      int ret = INTERNAL_SYSCALL_CALL (rseq, rseq_get_area(),
                                       GLRO (dl_tls_rseq_size),
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
