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

#serial 20090822

## _MM_CHECK_GNU_MAKE
##
## Implementation of MM_CHECK_GNU_MAKE.
##
m4_define([_MM_CHECK_GNU_MAKE],
[dnl
AC_PROVIDE([$0])[]dnl
AC_MSG_CHECKING([whether [$]{MAKE-make} supports GNU make features])
cat >conftest.make <<'_MMEOF'
override reverse = [$](2)[$](subst ,, )[$](1)
override result := [$](word 2,[$](call reverse,success,failure))
all: ; test '[$](result)' = success
.PHONY: all
_MMEOF
AS_IF([[$]{MAKE-make} -f conftest.make >&AS_MESSAGE_LOG_FD 2>&AS_MESSAGE_LOG_FD],
      [mm_gnu_make=yes], [mm_gnu_make=no])
rm -f conftest.make
AC_MSG_RESULT([$mm_gnu_make])
AS_IF([test "x$mm_gnu_make" != xyes],
      [AC_MSG_FAILURE([[The GNU make program is required to build $PACKAGE_NAME.]])])[]dnl
])

## MM_CHECK_GNU_MAKE
##
## Check whether the installed make program supports GNU make features.
## If the test fails, display an error message and abort.
##
AC_DEFUN([MM_CHECK_GNU_MAKE],
[dnl
AC_REQUIRE([_MM_PRE_INIT])[]dnl
AC_REQUIRE([_MM_CHECK_GNU_MAKE])[]dnl
])

## _MM_PATH_PERL
##
## Internal helper macro for MM_PATH_PERL.
##
m4_define([_MM_PATH_PERL],
[dnl
AC_PROVIDE([$0])[]dnl
AC_ARG_VAR([PERL], [path to Perl interpreter])[]dnl
AC_PATH_PROG([PERL], [perl], [perl])[]dnl
])

## MM_PATH_PERL
##
## Locate the Perl interpreter and set the substitution variable PERL
## to the full path to the perl executable if found, or to 'perl' if
## not found.  Also call AC_ARG_VAR() on the PERL variable.
##
AC_DEFUN([MM_PATH_PERL],
[dnl
AC_REQUIRE([_MM_PRE_INIT])[]dnl
AC_REQUIRE([_MM_PATH_PERL])[]dnl
])

## _MM_CHECK_PERL(min-version, [action-if-found], [action-if-not-found])
##
## Internal helper macro for MM_CHECK_PERL.
##
m4_define([_MM_CHECK_PERL],
[dnl
AS_IF([$PERL -e "require v$1; exit 0;" >&AS_MESSAGE_LOG_FD 2>&AS_MESSAGE_LOG_FD],
      [$2], m4_ifval([$2$3], [[$3]],
            [[AC_MSG_FAILURE([[At least Perl ]$1[ is required to build $PACKAGE_NAME.]])]]))[]dnl
])

## MM_CHECK_PERL([min-version], [action-if-found], [action-if-not-found])
##
## Run MM_PATH_PERL and then check whether the Perl interpreter can be
## executed and whether it meets the version requirement of <min-version>
## or later.  Execute <action-if-found> on success, otherwise execute
## <action-if-not-found>.  The default value of <min-version> is 5.6.0
## if the argument is empty.
##
AC_DEFUN([MM_CHECK_PERL],
[dnl
AC_REQUIRE([_MM_PRE_INIT])[]dnl
AC_REQUIRE([_MM_PATH_PERL])[]dnl
_MM_CHECK_PERL(m4_ifval([$1], [[$1]], [[5.6.0]]), [$2], [$3])[]dnl
])

## MM_PKG_CONFIG_SUBST(variable, arguments, [action-if-found], [action-if-not-found])
##
## Run the pkg-config utility with the specified command-line <arguments>
## and capture its standard output in the named shell <variable>.  If the
## command exited successfully, execute <action-if-found> in the shell if
## specified.  If the command failed, run <action-if-not-found> if given,
## otherwise ignore the error.
##
AC_DEFUN([MM_PKG_CONFIG_SUBST],
[dnl
m4_assert([$# >= 2])[]dnl
AC_REQUIRE([_MM_PRE_INIT])[]dnl
AC_REQUIRE([PKG_PROG_PKG_CONFIG])[]dnl
AC_MSG_CHECKING([for $1])
dnl
AS_IF([test -z "[$]{$1+set}"],
      [$1=`$PKG_CONFIG $2 2>&AS_MESSAGE_LOG_FD`
       AS_IF([test "[$]?" -eq 0], [$3], [$4])])
dnl
AC_MSG_RESULT([[$]$1])
AC_SUBST([$1])[]dnl
])
