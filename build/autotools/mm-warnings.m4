## Copyright (c) 2009  Openismus GmbH  <http://www.openismus.com/>
##
## This file is part of mm-common.
##
## mm-common is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published
## by the Free Software Foundation, either version 2 of the License,
## or (at your option) any later version.
##
## mm-common is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with mm-common.  If not, see <http://www.gnu.org/licenses/>.

#serial 20091103

## _MM_ARG_ENABLE_WARNINGS_OPTION
##
## Implementation helper macro of MM_ARG_ENABLE_WARNINGS().  Pulled in
## through AC_REQUIRE() so that it is only expanded once.
##
m4_define([_MM_ARG_ENABLE_WARNINGS_OPTION],
[dnl
AC_PROVIDE([$0])[]dnl
AC_ARG_ENABLE([warnings],
              [AS_HELP_STRING([[--enable-warnings[=min|max|fatal|no]]],
                              [set compiler pedantry level [default=min]])],
              [mm_enable_warnings=$enableval],
              [mm_enable_warnings=min])[]dnl
])

## MM_ARG_ENABLE_WARNINGS(variable, min-flags, max-flags, [deprecation-prefixes])
##
## Provide the --enable-warnings configure argument, set to "min" by default.
## <min-flags> and <max-flags> should be space-separated lists of compiler
## warning flags to use with --enable-warnings=min or --enable-warnings=max,
## respectively.  Warning level "fatal" is the same as "max" but in addition
## enables -Werror mode.
##
## If not empty, <deprecation-prefixes> should be a list of module prefixes
## which is expanded to -D<module>_DISABLE_DEPRECATED flags if fatal warnings
## are enabled, too.
##
## For instance, your configure.ac file might use the macro like this:
##
##   MM_ARG_ENABLE_WARNINGS([EXAMPLE_WFLAGS],
##                          [-Wall],
##                          [-pedantic -Wall -Wextra],
##                          [G PANGO ATK GDK GDK_PIXBUF GTK])
##
## Your Makefile.am could then contain a line such as this:
##
##   AM_CFLAGS = $(EXAMPLE_WFLAGS)
##
## In order to determine the warning options to use with the C++ compiler,
## call AC_LANG([C++]) first to change the current language.  If different
## output variables are used, it is also fine to call MM_ARG_ENABLE_WARNINGS
## repeatedly, once for each language setting.
##
## You may force people to fix warnings when creating release tarballs by
## adding this line to your Makefile.am:
##
##   DISTCHECK_CONFIGURE_FLAGS = --enable-warnings=fatal
##
AC_DEFUN([MM_ARG_ENABLE_WARNINGS],
[dnl
m4_assert([$# >= 3])[]dnl
AC_REQUIRE([_MM_PRE_INIT])[]dnl
AC_REQUIRE([_MM_ARG_ENABLE_WARNINGS_OPTION])[]dnl
dnl
AS_CASE([$ac_compile],
        [[*'$CXXFLAGS '*]], [mm_lang='C++' mm_cc=$CXX mm_conftest="conftest.[$]{ac_ext-cc}"],
        [[*'$CFLAGS '*]],   [mm_lang=C mm_cc=$CC mm_conftest="conftest.[$]{ac_ext-c}"],
        [AC_MSG_ERROR([[current language is neither C nor C++]])])
dnl
AC_MSG_CHECKING([which $mm_lang compiler warning flags to use])
m4_ifval([$4], [mm_deprecation_flags=
])mm_tested_flags=
dnl
AS_CASE([$mm_enable_warnings],
        [no],    [mm_warning_flags=],
        [max],   [mm_warning_flags="$3"],
        [fatal], [mm_warning_flags="$3 -Werror"[]m4_ifval([$4], [
         for mm_prefix in $4
         do
           mm_deprecation_flags="$mm_deprecation_flags-D[$]{mm_prefix}_DISABLE_DEPRECATED "
         done])],
        [mm_warning_flags="$2"])
dnl
AS_IF([test "x$mm_warning_flags" != x],
[
  # Keep in mind that the dummy source must be devoid of any
  # problems that might cause diagnostics.
  AC_LANG_CONFTEST([AC_LANG_SOURCE([[
int main(int argc, char** argv) { return !argv ? 0 : argc; }
]])])
  for mm_flag in $mm_warning_flags
  do
    # Test whether the compiler accepts the flag.  Look at standard output,
    # since GCC only shows a warning message if an option is not supported.
    mm_cc_out=`$mm_cc $mm_tested_flags $mm_flag -c "$mm_conftest" 2>&1 || echo failed`
    rm -f "conftest.[$]{OBJEXT-o}"

    AS_IF([test "x$mm_cc_out" = x],
          [AS_IF([test "x$mm_tested_flags" = x],
                 [mm_tested_flags=$mm_flag],
                 [mm_tested_flags="$mm_tested_flags $mm_flag"])],
[cat <<_MMEOF >&AS_MESSAGE_LOG_FD
$mm_cc: $mm_cc_out
_MMEOF
])
  done
  rm -f "$mm_conftest"
])
mm_all_flags=m4_ifval([$4], [$mm_deprecation_flags])$mm_tested_flags
AC_SUBST([$1], [$mm_all_flags])
dnl
test "x$mm_all_flags" != x || mm_all_flags=none
AC_MSG_RESULT([$mm_all_flags])[]dnl
])
