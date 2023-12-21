/* Restartable Sequences internal API. aarch64 macros.
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
#define RSEQ_HAS_LOAD32_LOAD32_RELAXED 1
static __always_inline int
rseq_load32_load32_relaxed(uint32_t *dst1, uint32_t *src1,
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
		: [rseq_cs]		"m" (rseq_get_area()->rseq_cs),
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
