/* Check DTAUDIT and vDSO interaction.
   Copyright (C) 2021 Free Software Foundation, Inc.
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

#include <link.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/auxv.h>

unsigned int
la_version (unsigned int version)
{
  return LAV_CURRENT;
}

unsigned int
la_objopen (struct link_map *map, Lmid_t lmid, uintptr_t *cookie)
{
  {
    const char *p = strrchr (map->l_name, '/');
    const char *l_name = p == NULL ? map->l_name : p + 1;
    if (strcmp (l_name, "tst-audit22") == 0)
      fprintf (stderr, "mainapp found\n");
  }

  if (map->l_addr == getauxval (AT_SYSINFO_EHDR))
    fprintf (stderr, "vdso found: %" PRIxPTR "\n", (uintptr_t) map->l_addr);

  return LA_FLG_BINDFROM | LA_FLG_BINDTO;
}

#if __ELF_NATIVE_CLASS == 64
uintptr_t
la_symbind64 (Elf64_Sym *sym, unsigned int ndx,
	      uintptr_t *refcook, uintptr_t *defcook,
	      unsigned int *flags, const char *symname)
{
  fprintf (stderr, "%s: %s\n", __func__, symname);
  return sym->st_value;
}
#else
uintptr_t
la_symbind32 (Elf32_Sym *sym, unsigned int ndx,
	      uintptr_t *refcook, uintptr_t *defcook,
	      unsigned int *flags, const char *symname)
{
  fprintf (stderr, "%s: %s\n", __func__, symname);
  return sym->st_value;
}
#endif