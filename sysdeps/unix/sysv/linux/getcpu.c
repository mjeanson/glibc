/* Copyright (C) 2007-2023 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

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

#include <errno.h>
#include <sched.h>
#include <sysdep.h>
#include <sysdep-vdso.h>
#include <rseq-internal.h>

static int
vsyscall_getcpu (unsigned int *cpu, unsigned int *node)
{
#ifdef HAVE_GETCPU_VSYSCALL
  return INLINE_VSYSCALL (getcpu, 3, cpu, node, NULL);
#else
  return INLINE_SYSCALL_CALL (getcpu, cpu, node, NULL);
#endif
}

#ifdef RSEQ_SIG
int
__getcpu (unsigned int *cpu, unsigned int *node)
{
# ifdef RSEQ_HAS_LOAD32_LOAD32_RELAXED
  /* Check if rseq is registered.  */
  if (__glibc_likely (rseq_is_registered() && rseq_node_id_available()))
  {
    struct rseq_area *rseq_area = rseq_get_area();

    if (rseq_load32_load32_relaxed(cpu, &rseq_area->cpu_id,
      		      node, &rseq_area->node_id) == 0)
    {
      /* The critical section was not aborted, return 0.  */
      return 0;
    }
  }
# endif

  return vsyscall_getcpu (cpu, node);
}
#else /* RSEQ_SIG */
int
__getcpu (unsigned int *cpu, unsigned int *node)
{
  return vsyscall_getcpu (cpu, node);
}
#endif /* RSEQ_SIG */
weak_alias (__getcpu, getcpu)
libc_hidden_def (__getcpu)
