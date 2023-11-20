/* RSEQ defines for the dynamic linker. Generic version.
   Copyright (C) 2023 Free Software Foundation, Inc.
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

/* Minimum size of the rseq area.  */
#define TLS_DL_RSEQ_MIN_SIZE 32

/* Minimum feature size of the rseq area.  */
#define TLS_DL_RSEQ_MIN_FEATURE_SIZE 20

/* Minimum size of the rseq area alignment.  */
#define TLS_DL_RSEQ_MIN_ALIGN 32
