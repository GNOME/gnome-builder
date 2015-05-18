#
# SYNOPSIS
#
#   GB_ENABLE_RDTSCP
#
# DESCRIPTION
#
#   Enables support for rdtscp counters as supported by egg-counter.
#
#   Note: This may or may not override some variables such as CFLAGS.
#
# LICENSE
#
#   Copyright (c) 2015 Christian Hergert <christian@hergert.me>
#
#   This program is free software: you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by the
#   Free Software Foundation, either version 3 of the License, or (at your
#   option) any later version.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
#   Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program. If not, see <http://www.gnu.org/licenses/>.
#
#   As a special exception, the respective Autoconf Macro's copyright owner
#   gives unlimited permission to copy, distribute and modify the configure
#   scripts that are the output of Autoconf when processing the Macro. You
#   need not follow the terms of the GNU General Public License when using
#   or distributing such scripts, even though portions of the text of the
#   Macro appear in them. The GNU General Public License (GPL) does govern
#   all other use of the material that constitutes the Autoconf Macro.
#
#   This special exception to the GPL applies to versions of the Autoconf
#   Macro released by the Autoconf Archive. When you make and distribute a
#   modified version of the Autoconf Macro, you may extend this special
#   exception to the GPL to apply to your modified version as well.

#serial 1

AC_DEFUN([GB_ENABLE_RDTSCP], [
	AC_ARG_ENABLE([rdtscp], [AC_HELP_STRING([--enable-rdtscp], [turn on rdtscp optimizations [default=no]])],
				  [enable_rdtscp=$enableval], [enable_rdtscp=no])

	AS_IF([test "$enable_rdtscp" = "yes"], [
		AC_MSG_CHECKING([for fast counters with rdtscp])
		AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <x86intrin.h>
int main (int argc, char *argv[]) { int cpu; __builtin_ia32_rdtscp (&cpu); return 0; }
]])],
		              [enable_rdtscp=yes],
		              [enable_rdtscp=no])
		AC_MSG_RESULT(["$enable_rdtscp"])
	])

	AS_CASE(["$host"],
	        [*-*-linux*],[CFLAGS="$CFLAGS -D_GNU_SOURCE"],
	        [])
])

