/* Define symbols used by rseq.
   Copyright (C) 2024 Free Software Foundation, Inc.
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

#include <rseq-sizes.h>
#include <sysdep.h>

/* Some targets define a macro to denote the zero register.  */
#undef zero

/* Define 2 symbols, __rseq_size is public const and _rseq_size, which is an
   alias of __rseq_size, but hidden and writable for internal use.  */

	.globl	__rseq_size
	.type	__rseq_size, %object
	.size	__rseq_size, RSEQ_SIZE_SIZE
	.hidden _rseq_size
	.globl	_rseq_size
	.type	_rseq_size, %object
	.size	_rseq_size, RSEQ_SIZE_SIZE
	.section .data.rel.ro
	.balign	RSEQ_SIZE_ALIGN
__rseq_size:
_rseq_size:
	.zero	RSEQ_SIZE_SIZE

/* Define 2 symbols, __rseq_feature_size is public const and
   _rseq_feature_size, which is an alias of __rseq_feature_size, but hidden and
   writable for internal use.  */

	.globl	__rseq_feature_size
	.type	__rseq_feature_size, %object
	.size	__rseq_feature_size, RSEQ_FEATURE_SIZE_SIZE
	.hidden _rseq_feature_size
	.globl	_rseq_feature_size
	.type	_rseq_feature_size, %object
	.size	_rseq_feature_size, RSEQ_FEATURE_SIZE_SIZE
	.section .data.rel.ro
	.balign	RSEQ_FEATURE_SIZE_ALIGN
__rseq_feature_size:
_rseq_feature_size:
	.zero	RSEQ_FEATURE_SIZE_SIZE

/* Define 2 symbols, __rseq_offset is public const and _rseq_offset, which is an
   alias of __rseq_offset, but hidden and writable for internal use.  */

	.globl	__rseq_offset
	.type	__rseq_offset, %object
	.size	__rseq_offset, RSEQ_OFFSET_SIZE
	.hidden _rseq_offset
	.globl	_rseq_offset
	.type	_rseq_offset, %object
	.size	_rseq_offset, RSEQ_OFFSET_SIZE
	.section .data.rel.ro
	.balign	RSEQ_OFFSET_ALIGN
__rseq_offset:
_rseq_offset:
	.zero	RSEQ_OFFSET_SIZE
