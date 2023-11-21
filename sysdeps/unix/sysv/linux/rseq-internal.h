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
#include <stdint.h>

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

#ifdef __x86_64__

/*
 * RSEQ_SIG is used with the following reserved undefined instructions, which
 * trap in user-space:
 *
 * x86-32:    0f b9 3d 53 30 05 53      ud1    0x53053053,%edi
 * x86-64:    0f b9 3d 53 30 05 53      ud1    0x53053053(%rip),%edi
 */
#define RSEQ_SIG	0x53053053

#define RSEQ_ASM_TP_SEGMENT	%%fs

#define RSEQ_ASM_STORE_CS_CLOBBER	, "rax"

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
static inline __attribute__((always_inline))
int rseq_load32_load32_relaxed(uint32_t *dst1, uint32_t *src1,
			       uint32_t *dst2, uint32_t *src2)
{
	__asm__ __volatile__ goto (
		RSEQ_ASM_DEFINE_TABLE(3, 1f, 2f, 4f) /* start, commit, abort */
		/* Start rseq by storing table entry pointer into rseq_cs. */
		RSEQ_ASM_STORE_RSEQ_CS(1, 3b, RSEQ_ASM_TP_SEGMENT:RSEQ_CS_OFFSET(%[rseq_offset]))
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
		: "memory", "cc", "ebx", "ecx"
		  RSEQ_ASM_STORE_CS_CLOBBER
		: abort
	);
	rseq_after_asm_goto();
	return 0;
abort:
	rseq_after_asm_goto();
	return -1;
}

#elif defined (__AARCH64EL__)

struct rseq_abi {
	__u32 cpu_id_start;
	__u32 cpu_id;
	union {
		__u64 ptr64;

		/*
		 * The "arch" field provides architecture accessor for
		 * the ptr field based on architecture pointer size and
		 * endianness.
		 */
		struct {
#ifdef __LP64__
			__u64 ptr;
#elif defined(__BYTE_ORDER) ? (__BYTE_ORDER == __BIG_ENDIAN) : defined(__BIG_ENDIAN)
			__u32 padding;		/* Initialized to zero. */
			__u32 ptr;
#else
			__u32 ptr;
			__u32 padding;		/* Initialized to zero. */
#endif
		} arch;
	} rseq_cs;

	__u32 flags;
	__u32 node_id;
	__u32 mm_cid;

	/*
	 * Flexible array member at end of structure, after last feature field.
	 */
	char end[];
} __attribute__((aligned(4 * sizeof(__u64))));

static inline struct rseq_abi *rseq_get_abi(void)
{
	return (struct rseq_abi *) ((uintptr_t) __builtin_thread_pointer() + __rseq_offset);
}

/*
 * aarch64 -mbig-endian generates mixed endianness code vs data:
 * little-endian code and big-endian data. Ensure the RSEQ_SIG signature
 * matches code endianness.
 */
#define RSEQ_SIG_CODE	0xd428bc00	/* BRK #0x45E0.  */

#ifdef __AARCH64EB__
#define RSEQ_SIG_DATA	0x00bc28d4	/* BRK #0x45E0.  */
#else
#define RSEQ_SIG_DATA	RSEQ_SIG_CODE
#endif

#define RSEQ_SIG	RSEQ_SIG_DATA

#define RSEQ_ASM_TMP_REG32	"w15"
#define RSEQ_ASM_TMP_REG	"x15"
#define RSEQ_ASM_TMP_REG_2	"x14"

#define __RSEQ_ASM_DEFINE_TABLE(label, version, flags, start_ip,		\
				post_commit_offset, abort_ip)			\
	"	.pushsection	__rseq_cs, \"aw\"\n"				\
	"	.balign	32\n"							\
	__rseq_str(label) ":\n"							\
	"	.long	" __rseq_str(version) ", " __rseq_str(flags) "\n"	\
	"	.quad	" __rseq_str(start_ip) ", "				\
			  __rseq_str(post_commit_offset) ", "			\
			  __rseq_str(abort_ip) "\n"				\
	"	.popsection\n\t"						\
	"	.pushsection __rseq_cs_ptr_array, \"aw\"\n"				\
	"	.quad " __rseq_str(label) "b\n"					\
	"	.popsection\n"

#define RSEQ_ASM_DEFINE_TABLE(label, start_ip, post_commit_ip, abort_ip)	\
	__RSEQ_ASM_DEFINE_TABLE(label, 0x0, 0x0, start_ip,			\
				(post_commit_ip - start_ip), abort_ip)

/*
 * Exit points of a rseq critical section consist of all instructions outside
 * of the critical section where a critical section can either branch to or
 * reach through the normal course of its execution. The abort IP and the
 * post-commit IP are already part of the __rseq_cs section and should not be
 * explicitly defined as additional exit points. Knowing all exit points is
 * useful to assist debuggers stepping over the critical section.
 */
#define RSEQ_ASM_DEFINE_EXIT_POINT(start_ip, exit_ip)				\
	"	.pushsection __rseq_exit_point_array, \"aw\"\n"			\
	"	.quad " __rseq_str(start_ip) ", " __rseq_str(exit_ip) "\n"	\
	"	.popsection\n"

#define RSEQ_ASM_STORE_RSEQ_CS(label, cs_label, rseq_cs)			\
	"	adrp	" RSEQ_ASM_TMP_REG ", " __rseq_str(cs_label) "\n"	\
	"	add	" RSEQ_ASM_TMP_REG ", " RSEQ_ASM_TMP_REG		\
			", :lo12:" __rseq_str(cs_label) "\n"			\
	"	str	" RSEQ_ASM_TMP_REG ", %[" __rseq_str(rseq_cs) "]\n"	\
	__rseq_str(label) ":\n"

#define RSEQ_ASM_DEFINE_ABORT(label, abort_label)				\
	"	b	222f\n"							\
	"	.inst 	"	__rseq_str(RSEQ_SIG_CODE) "\n"			\
	__rseq_str(label) ":\n"							\
	"	b	%l[" __rseq_str(abort_label) "]\n"			\
	"222:\n"

#define RSEQ_ASM_OP_STORE(value, var)						\
	"	str	%[" __rseq_str(value) "], %[" __rseq_str(var) "]\n"

#define RSEQ_ASM_OP_STORE_RELEASE(value, var)					\
	"	stlr	%[" __rseq_str(value) "], %[" __rseq_str(var) "]\n"

#define RSEQ_ASM_OP_FINAL_STORE(value, var, post_commit_label)			\
	RSEQ_ASM_OP_STORE(value, var)						\
	__rseq_str(post_commit_label) ":\n"

#define RSEQ_ASM_OP_FINAL_STORE_RELEASE(value, var, post_commit_label)		\
	RSEQ_ASM_OP_STORE_RELEASE(value, var)					\
	__rseq_str(post_commit_label) ":\n"

#define RSEQ_ASM_OP_CMPEQ(var, expect, label)					\
	"	ldr	" RSEQ_ASM_TMP_REG ", %[" __rseq_str(var) "]\n"		\
	"	sub	" RSEQ_ASM_TMP_REG ", " RSEQ_ASM_TMP_REG		\
			", %[" __rseq_str(expect) "]\n"				\
	"	cbnz	" RSEQ_ASM_TMP_REG ", " __rseq_str(label) "\n"

#define RSEQ_ASM_OP_CMPEQ32(var, expect, label)					\
	"	ldr	" RSEQ_ASM_TMP_REG32 ", %[" __rseq_str(var) "]\n"	\
	"	sub	" RSEQ_ASM_TMP_REG32 ", " RSEQ_ASM_TMP_REG32		\
			", %w[" __rseq_str(expect) "]\n"			\
	"	cbnz	" RSEQ_ASM_TMP_REG32 ", " __rseq_str(label) "\n"

#define RSEQ_ASM_OP_CMPNE(var, expect, label)					\
	"	ldr	" RSEQ_ASM_TMP_REG ", %[" __rseq_str(var) "]\n"		\
	"	sub	" RSEQ_ASM_TMP_REG ", " RSEQ_ASM_TMP_REG		\
			", %[" __rseq_str(expect) "]\n"				\
	"	cbz	" RSEQ_ASM_TMP_REG ", " __rseq_str(label) "\n"

#define RSEQ_ASM_CMP_CPU_ID(cpu_id, current_cpu_id, label)			\
	RSEQ_ASM_OP_CMPEQ32(current_cpu_id, cpu_id, label)

#define RSEQ_ASM_OP_R_LOAD(var)							\
	"	ldr	" RSEQ_ASM_TMP_REG ", %[" __rseq_str(var) "]\n"

#define RSEQ_ASM_OP_R_STORE(var)						\
	"	str	" RSEQ_ASM_TMP_REG ", %[" __rseq_str(var) "]\n"

#define RSEQ_ASM_OP_R_LOAD32(var)						\
	"	ldr	" RSEQ_ASM_TMP_REG32 ", %[" __rseq_str(var) "]\n"

#define RSEQ_ASM_OP_R_STORE32(var)						\
	"	str	" RSEQ_ASM_TMP_REG32 ", %[" __rseq_str(var) "]\n"

#define RSEQ_ASM_OP_R_LOAD_OFF(offset)						\
	"	ldr	" RSEQ_ASM_TMP_REG ", [" RSEQ_ASM_TMP_REG		\
			", %[" __rseq_str(offset) "]]\n"

#define RSEQ_ASM_OP_R_ADD(count)						\
	"	add	" RSEQ_ASM_TMP_REG ", " RSEQ_ASM_TMP_REG		\
			", %[" __rseq_str(count) "]\n"

#define RSEQ_ASM_OP_R_FINAL_STORE(var, post_commit_label)			\
	"	str	" RSEQ_ASM_TMP_REG ", %[" __rseq_str(var) "]\n"		\
	__rseq_str(post_commit_label) ":\n"

#define RSEQ_ASM_OP_R_FINAL_STORE32(var, post_commit_label)			\
	"	str	" RSEQ_ASM_TMP_REG32 ", %[" __rseq_str(var) "]\n"	\
	__rseq_str(post_commit_label) ":\n"

#define RSEQ_ASM_OP_R_BAD_MEMCPY(dst, src, len)					\
	"	cbz	%[" __rseq_str(len) "], 333f\n"				\
	"	mov	" RSEQ_ASM_TMP_REG_2 ", %[" __rseq_str(len) "]\n"	\
	"222:	sub	" RSEQ_ASM_TMP_REG_2 ", " RSEQ_ASM_TMP_REG_2 ", #1\n"	\
	"	ldrb	" RSEQ_ASM_TMP_REG32 ", [%[" __rseq_str(src) "]"	\
			", " RSEQ_ASM_TMP_REG_2 "]\n"				\
	"	strb	" RSEQ_ASM_TMP_REG32 ", [%[" __rseq_str(dst) "]"	\
			", " RSEQ_ASM_TMP_REG_2 "]\n"				\
	"	cbnz	" RSEQ_ASM_TMP_REG_2 ", 222b\n"				\
	"333:\n"

/*
 * Load @src1 (32-bit) into @dst1 and load @src2 (32-bit) into @dst2.
 */
static inline __attribute__((always_inline))
int rseq_load32_load32_relaxed(uint32_t *dst1, uint32_t *src1,
			       uint32_t *dst2, uint32_t *src2)
{
	__asm__ __volatile__ goto (
		RSEQ_ASM_DEFINE_TABLE(1, 2f, 3f, 4f)
		RSEQ_ASM_STORE_RSEQ_CS(2, 1b, rseq_cs)
		RSEQ_ASM_OP_R_LOAD32(src1)
		RSEQ_ASM_OP_R_STORE32(dst1)
		RSEQ_ASM_OP_R_LOAD32(src2)
		RSEQ_ASM_OP_R_FINAL_STORE32(dst2, 3)
		RSEQ_ASM_DEFINE_ABORT(4, abort)
		: /* gcc asm goto does not allow outputs */
		: [rseq_cs]		"m" (rseq_get_abi()->rseq_cs.arch.ptr),
		  [dst1]		"Qo" (*dst1),
		  [dst2]		"Qo" (*dst2),
		  [src1]		"Qo" (*src1),
		  [src2]		"Qo" (*src2)
		: "memory", RSEQ_ASM_TMP_REG
		: abort
	);
	rseq_after_asm_goto();
	return 0;
abort:
	rseq_after_asm_goto();
	return -1;
}

#else

#error "Unsupported architecture."

#endif

/* rseq area registered with the kernel.  Use a custom definition
   here to isolate from kernel struct rseq changes.  The
   implementation of sched_getcpu needs acccess to the cpu_id field;
   the other fields are unused and not included here.  */
struct rseq_area
{
  uint32_t cpu_id_start;
  uint32_t cpu_id;
  uint64_t rseq_cs;
  uint32_t flags;
  uint32_t node_id;
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
