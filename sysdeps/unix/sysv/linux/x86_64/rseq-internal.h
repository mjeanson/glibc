/* Restartable Sequences internal API. x86_64 macros.
   Copyright (C) 2023 Free Software Foundation, Inc.

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

#include <sysdeps/unix/sysv/linux/rseq-internal.h>

#define __RSEQ_ASM_DEFINE_TABLE(label, version, flags,			\
				start_ip, post_commit_offset, abort_ip)	\
		".pushsection __rseq_cs, \"aw\"\n\t"			\
		".balign 32\n\t"					\
		__rseq_str(label) ":\n\t"				\
		".long " __rseq_str(version) ", " __rseq_str(flags) "\n\t" \
		".quad " __rseq_str(start_ip) ", " __rseq_str(post_commit_offset) ", " __rseq_str(abort_ip) "\n\t" \
		".popsection\n\t"					\
		".pushsection __rseq_cs_ptr_array, \"aw\"\n\t"		\
		".quad " __rseq_str(label) "b\n\t"			\
		".popsection\n\t"

#define RSEQ_ASM_DEFINE_TABLE(label, start_ip, post_commit_ip, abort_ip) \
	__RSEQ_ASM_DEFINE_TABLE(label, 0x0, 0x0, start_ip,		\
				(post_commit_ip - start_ip), abort_ip)

/*
 * Exit points of a rseq critical section consist of all instructions outside
 * of the critical section where a critical section can either branch to or
 * reach through the normal course of its execution. The abort IP and the
 * post-commit IP are already part of the __rseq_cs section and should not be
 * explicitly defined as additional exit points. Knowing all exit points is
 * useful to assist debuggers stepping over the critical section.
 */
#define RSEQ_ASM_DEFINE_EXIT_POINT(start_ip, exit_ip)			\
		".pushsection __rseq_exit_point_array, \"aw\"\n\t"	\
		".quad " __rseq_str(start_ip) ", " __rseq_str(exit_ip) "\n\t" \
		".popsection\n\t"

#define RSEQ_ASM_STORE_RSEQ_CS(label, cs_label, rseq_cs)		\
		"leaq " __rseq_str(cs_label) "(%%rip), %%rax\n\t"	\
		"movq %%rax, " __rseq_str(rseq_cs) "\n\t"		\
		__rseq_str(label) ":\n\t"

#define RSEQ_ASM_CMP_CPU_ID(cpu_id, current_cpu_id, label)		\
		"cmpl %[" __rseq_str(cpu_id) "], " __rseq_str(current_cpu_id) "\n\t" \
		"jnz " __rseq_str(label) "\n\t"

#define RSEQ_ASM_DEFINE_ABORT(label, teardown, abort_label)		\
		".pushsection __rseq_failure, \"ax\"\n\t"		\
		/* Disassembler-friendly signature: ud1 <sig>(%rip),%edi. */ \
		".byte 0x0f, 0xb9, 0x3d\n\t"				\
		".long " __rseq_str(RSEQ_SIG) "\n\t"			\
		__rseq_str(label) ":\n\t"				\
		teardown						\
		"jmp %l[" __rseq_str(abort_label) "]\n\t"		\
		".popsection\n\t"

#define RSEQ_ASM_DEFINE_CMPFAIL(label, teardown, cmpfail_label)		\
		".pushsection __rseq_failure, \"ax\"\n\t"		\
		__rseq_str(label) ":\n\t"				\
		teardown						\
		"jmp %l[" __rseq_str(cmpfail_label) "]\n\t"		\
		".popsection\n\t"

/*
 * Load @src1 (32-bit) into @dst1 and load @src2 (32-bit) into @dst2.
 */
#define RSEQ_HAS_LOAD32_LOAD32_RELAXED 1
static __always_inline int
rseq_load32_load32_relaxed(uint32_t *dst1, uint32_t *src1,
			       uint32_t *dst2, uint32_t *src2)
{
	__asm__ __volatile__ goto (
		RSEQ_ASM_DEFINE_TABLE(3, 1f, 2f, 4f) /* start, commit, abort */
		/* Start rseq by storing table entry pointer into rseq_cs. */
		RSEQ_ASM_STORE_RSEQ_CS(1, 3b, %%fs:RSEQ_CS_OFFSET(%[rseq_offset]))
		"movl %[src1], %%ebx\n\t"
		"movl %[src2], %%ecx\n\t"
		"movl %%ebx, %[dst1]\n\t"
		/* final store */
		"movl %%ecx, %[dst2]\n\t"
		"2:\n\t"
		RSEQ_ASM_DEFINE_ABORT(4, "", abort)
		: /* gcc asm goto does not allow outputs */
		: [rseq_offset]		"r" (__rseq_offset),
		  /* final store input */
		  [dst1]		"m" (*dst1),
		  [dst2]		"m" (*dst2),
		  [src1]		"m" (*src1),
		  [src2]		"m" (*src2)
		: "memory", "cc", "ebx", "ecx", "rax"
		: abort
	);
	rseq_after_asm_goto();
	return 0;
abort:
	rseq_after_asm_goto();
	return -1;
}
