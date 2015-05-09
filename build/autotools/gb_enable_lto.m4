#
# SYNOPSIS
#
#   GB_ENABLE_LTO
#
# DESCRIPTION
#
#   Enables support for link time optimization. $LTO_CFLAGS and $enable_lto
#   is exported for you to use.
#
#   Note: This may or may not override some variables such as AR,
#         RANLIB, or NM.
#
# LICENSE
#
#   Copyright (c) 2015 Patrick Griffis <tingping@tingping.se>
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

#serial 4

AC_DEFUN([GB_ENABLE_LTO], [
	AC_ARG_ENABLE([lto], [AC_HELP_STRING([--enable-lto], [turn on link time optimizations [default=no]])],
				  [enable_lto=$enableval], [enable_lto=no])

	AX_REQUIRE_DEFINED([AX_CHECK_COMPILE_FLAG])
	AX_REQUIRE_DEFINED([AX_COMPILER_VENDOR])

	AS_IF([test "$enable_lto" = "yes"], [
		AX_COMPILER_VENDOR
		AS_CASE([$ax_cv_c_compiler_vendor],
		[gnu], [
			AC_PATH_PROG([GCC_AR], [gcc-ar], [no])
			AC_PATH_PROG([GCC_NM], [gcc-nm], [no])
			AC_PATH_PROG([GCC_RANLIB], [gcc-ranlib], [no])
			AX_CHECK_COMPILE_FLAG([-flto], [LTO_CFLAGS=-flto], [LTO_CLFAGS=no])
			AS_IF([test "$GCC_AR" = "no" || test "$GCC_NM" = "no" || test "$GCC_RANLIB" = "no" || test "$LTO_CFLAGS" = "no"], [
				enable_lto=no
			], [
				dnl this just overwrites variables which may not be ideal..
				AR="$GCC_AR"
				NM="$GCC_NM"
				RANLIB="$GCC_RANLIB"
				AC_SUBST([AR])
				AC_SUBST([NM])
				AC_SUBST([RANLIB])
				AC_SUBST([LTO_CFLAGS])
			])
		], [
			AC_MSG_ERROR([Currently only gcc is supported for --enable-lto])
		])
	])
])

