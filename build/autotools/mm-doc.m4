## Copyright (c) 2009, 2010, 2011  Openismus GmbH  <http://www.openismus.com/>
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

#serial 20110327

## _MM_CONFIG_DOCTOOL_DIR
##
## Query pkg-config for the location of the documentation utilities
## shared by the GNOME C++ bindings.  This is the code path invoked
## from MM_CONFIG_DOCTOOL_DIR when called without a directory name.
##
m4_define([_MM_CONFIG_DOCTOOL_DIR],
[dnl
AC_PROVIDE([$0])[]dnl
AC_REQUIRE([PKG_PROG_PKG_CONFIG])[]dnl
dnl
AC_MSG_CHECKING([location of documentation utilities])
AS_IF([test "x$MMDOCTOOLDIR" = x],
[
  MMDOCTOOLDIR=`$PKG_CONFIG --variable=doctooldir mm-common-util 2>&AS_MESSAGE_LOG_FD`
  AS_IF([test "[$]?" -ne 0],
        [AC_MSG_ERROR([[not found
The required module mm-common-util could not be found on this system.  If you
are running a binary distribution and the mm-common package is installed,
make sure that any separate development package for mm-common is installed
as well.  If you built mm-common yourself, it may be necessary to adjust
the PKG_CONFIG_PATH environment variable for pkg-config to find it.
]])])
])
AC_MSG_RESULT([$MMDOCTOOLDIR])[]dnl
])

## MM_CONFIG_DOCTOOL_DIR([directory])
##
## Define the location of the documentation utilities shared by the
## GNOME C++ binding modules.  If the <directory> argument is given,
## it should name a directory relative to the toplevel directory of
## the source tree.
##
## The directory name is used by mm-common-prepare as the destination
## for copying the required files into the source tree.  The files are not
## distributed if first parameter is empty.
##
AC_DEFUN([MM_CONFIG_DOCTOOL_DIR],
[dnl
AC_REQUIRE([_MM_PRE_INIT])[]dnl
AC_REQUIRE([MM_CHECK_GNU_MAKE])[]dnl
m4_ifval([$1], [MMDOCTOOLDIR='[$]{top_srcdir}/$1'], [AC_REQUIRE([_MM_CONFIG_DOCTOOL_DIR])])
AM_CONDITIONAL([DIST_DOCTOOLS], [test 'x$1' != 'x'])dnl
AC_SUBST([MMDOCTOOLDIR])[]dnl
])

## _MM_ARG_ENABLE_DOCUMENTATION
##
## Implementation of MM_ARG_ENABLE_DOCUMENTATION, pulled in indirectly
## through AC_REQUIRE() to make sure it is expanded only once.
##
m4_define([_MM_ARG_ENABLE_DOCUMENTATION],
[dnl
AC_PROVIDE([$0])[]dnl
dnl
AC_ARG_VAR([DOT], [path to dot utility])[]dnl
AC_ARG_VAR([DOXYGEN], [path to Doxygen utility])[]dnl
AC_ARG_VAR([XSLTPROC], [path to xsltproc utility])[]dnl
dnl
AC_PATH_PROG([DOT], [dot], [dot])
AC_PATH_PROG([DOXYGEN], [doxygen], [doxygen])
AC_PATH_PROG([XSLTPROC], [xsltproc], [xsltproc])
dnl
AC_ARG_ENABLE([documentation],
              [AS_HELP_STRING([--disable-documentation],
                              [do not build or install the documentation])],
              [ENABLE_DOCUMENTATION=$enableval],
              [ENABLE_DOCUMENTATION=auto])
AS_IF([test "x$ENABLE_DOCUMENTATION" != xno],
[
  mm_err=
  AS_IF([test "x$MMDOCTOOLDIR" = x], [mm_err='dnl
The mm-common-util module is available, but the installation of mm-common on this
machine is missing the shared documentation utilities of the GNOME C++
bindings.  It may be necessary to upgrade to a more recent release of
mm-common in order to build '$PACKAGE_NAME' and install the documentation.'],
        [test "x$PERL" = xperl], [mm_err='Perl is required for installing the documentation.'],
        [test "x$USE_MAINTAINER_MODE" != xno],
  [
    test "x$DOT" != xdot || mm_err=' dot'
    test "x$DOXYGEN" != xdoxygen || mm_err="$mm_err doxygen"
    test "x$XSLTPROC" != xxsltproc || mm_err="$mm_err xsltproc"
    test -z "$mm_err" || mm_err='The documentation cannot be generated because
not all of the required tools are available:'$mm_err
  ])
  AS_IF([test -z "$mm_err"], [ENABLE_DOCUMENTATION=yes],
        [test "x$ENABLE_DOCUMENTATION" = xyes], [AC_MSG_FAILURE([[$mm_err]])],
        [ENABLE_DOCUMENTATION=no; AC_MSG_WARN([[$mm_err]])])
])
AM_CONDITIONAL([ENABLE_DOCUMENTATION], [test "x$ENABLE_DOCUMENTATION" = xyes])
AC_SUBST([DOXYGEN_TAGFILES], [[]])
AC_SUBST([DOCINSTALL_FLAGS], [[]])[]dnl
])

## MM_ARG_ENABLE_DOCUMENTATION
##
## Provide the --disable-documentation configure option.  By default,
## the documentation will be included in the build.  If not explicitly
## disabled, also check whether the necessary tools are installed, and
## abort if any are missing.
##
## The tools checked for are Perl, dot, Doxygen and xsltproc.  The
## substitution variables PERL, DOT, DOXYGEN and XSLTPROC are set to
## the command paths, unless overridden in the user environment.
##
## If the package provides the --enable-maintainer-mode option, the
## tools dot, Doxygen and xsltproc are mandatory only when maintainer
## mode is enabled.  Perl is required for the installdox utility even
## if not in maintainer mode.
##
AC_DEFUN([MM_ARG_ENABLE_DOCUMENTATION],
[dnl
AC_BEFORE([$0], [MM_ARG_WITH_TAGFILE_DOC])[]dnl
AC_REQUIRE([_MM_PRE_INIT])[]dnl
AC_REQUIRE([MM_CONFIG_DOCTOOL_DIR])[]dnl
AC_REQUIRE([MM_PATH_PERL])[]dnl
AC_REQUIRE([_MM_ARG_ENABLE_DOCUMENTATION])[]dnl
])

## _MM_TR_URI(shell-expression)
##
## Internal macro that expands to a reusable shell construct which
## functions as a poor man's filesystem path to URI translator.
## The input <shell-expression> is expanded within double quotes.
##
m4_define([_MM_TR_URI],
[dnl
[`expr "X$1" : 'X\(.*[^\\/]\)[\\/]*' 2>&]AS_MESSAGE_LOG_FD[ |]dnl
[ sed 's|[\\]|/|g;s| |%20|g;s|^/|file:///|;s|^.:/|file:///&|' 2>&]AS_MESSAGE_LOG_FD[`]dnl
])

## _MM_ARG_WITH_TAGFILE_DOC(option-basename, pkg-variable, tagfilename, [module])
##
m4_define([_MM_ARG_WITH_TAGFILE_DOC],
[dnl
  AC_MSG_CHECKING([for $1 documentation])
  AC_ARG_WITH([$1-doc],
              [AS_HELP_STRING([[--with-$1-doc=[TAGFILE@]HTMLREFDIR]],
                              [Link to external $1 documentation]m4_ifval([$4], [[ [auto]]]))],
  [
    mm_htmlrefdir=`[expr "X@$withval" : '.*@\(.*\)' 2>&]AS_MESSAGE_LOG_FD`
    mm_tagname=`[expr "X/$withval" : '[^@]*[\\/]\([^\\/@]*\)@' 2>&]AS_MESSAGE_LOG_FD`
    mm_tagpath=`[expr "X$withval" : 'X\([^@]*\)@' 2>&]AS_MESSAGE_LOG_FD`
    test "x$mm_tagname" != x || mm_tagname="$3"
    test "x$mm_tagpath" != x || mm_tagpath=$mm_tagname[]dnl
  ], [
    mm_htmlrefdir=
    mm_tagname="$3"
    mm_tagpath=$mm_tagname[]dnl
  ])
  # Prepend working direcory if the tag file path starts with ./ or ../
  AS_CASE([$mm_tagpath], [[.[\\/]*|..[\\/]*]], [mm_tagpath=`pwd`/$mm_tagpath])

m4_ifval([$4], [dnl
  # If no local directory was specified, get the default from the .pc file
  AS_IF([test "x$mm_htmlrefdir" = x],
  [
    mm_htmlrefdir=`$PKG_CONFIG --variable=htmlrefdir "$4" 2>&AS_MESSAGE_LOG_FD`dnl
  ])
  # If the user specified a Web URL, allow it to override the public location
  AS_CASE([$mm_htmlrefdir], [[http://*|https://*]], [mm_htmlrefpub=$mm_htmlrefdir],
  [
    mm_htmlrefpub=`$PKG_CONFIG --variable=htmlrefpub "$4" 2>&AS_MESSAGE_LOG_FD`
    test "x$mm_htmlrefpub" != x || mm_htmlrefpub=$mm_htmlrefdir
    test "x$mm_htmlrefdir" != x || mm_htmlrefdir=$mm_htmlrefpub
  ])
  # The user-supplied tag-file name takes precedence if it includes the path
  AS_CASE([$mm_tagpath], [[*[\\/]*]],,
  [
    mm_doxytagfile=`$PKG_CONFIG --variable=doxytagfile "$4" 2>&AS_MESSAGE_LOG_FD`
    test "x$mm_doxytagfile" = x || mm_tagpath=$mm_doxytagfile
  ])
  # Remove trailing slashes and translate to URI
  mm_htmlrefpub=_MM_TR_URI([$mm_htmlrefpub])
])[]dnl
  mm_htmlrefdir=_MM_TR_URI([$mm_htmlrefdir])

  AC_MSG_RESULT([$mm_tagpath@$mm_htmlrefdir])

  AS_IF([test "x$USE_MAINTAINER_MODE" != xno && test ! -f "$mm_tagpath"],
        [AC_MSG_WARN([Doxygen tag file $3 not found])])
  AS_IF([test "x$mm_htmlrefdir" = x],
        [AC_MSG_WARN([Location of external $1 documentation not set])],
        [AS_IF([test "x$DOCINSTALL_FLAGS" = x],
               [DOCINSTALL_FLAGS="-l '$mm_tagname@$mm_htmlrefdir/'"],
               [DOCINSTALL_FLAGS="$DOCINSTALL_FLAGS -l '$mm_tagname@$mm_htmlrefdir/'"])])

  AS_IF([test "x$mm_$2" = x], [mm_val=$mm_tagpath], [mm_val="$mm_tagpath=$mm_$2"])
  AS_IF([test "x$DOXYGEN_TAGFILES" = x],
        [DOXYGEN_TAGFILES=[\]"$mm_val[\]"],
        [DOXYGEN_TAGFILES="$DOXYGEN_TAGFILES "[\]"$mm_val[\]"])[]dnl
])

## MM_ARG_WITH_TAGFILE_DOC(tagfilename, [module])
##
## Provide a --with-<tagfilebase>-doc=[/path/tagfile@]htmlrefdir configure
## option, which may be used to specify the location of a tag file and the
## path to the corresponding HTML reference documentation.  If the project
## provides the maintainer mode option and maintainer mode is not enabled,
## the user does not have to provide the full path to the tag file.  The
## full path is only required for rebuilding the documentation.
##
## If the optional <module> argument has been specified, and either the tag
## file or the HTML location have not been overridden by the user already,
## try to retrieve the missing paths automatically via pkg-config.  Also ask
## pkg-config for the URI to the online documentation, for use as the preset
## location when the documentation is generated.
##
## A warning message will be shown if the HTML path could not be determined.
## If maintainer mode is active, a warning is also displayed if the tag file
## could not be found.
##
## The results are appended to the substitution variables DOXYGEN_TAGFILES
## and DOCINSTALL_FLAGS, using the following format:
##
##  DOXYGEN_TAGFILES = "/path/tagfile=htmlrefpub" [...]
##  DOCINSTALL_FLAGS = -l 'tagfile@htmlrefdir' [...]
##
## The substitutions are intended to be used for the Doxygen configuration,
## and as argument list to the doc-install.pl or installdox utility.
##
AC_DEFUN([MM_ARG_WITH_TAGFILE_DOC],
[dnl
m4_assert([$# >= 1])[]dnl
m4_ifval([$2], [AC_REQUIRE([PKG_PROG_PKG_CONFIG])])[]dnl
AC_REQUIRE([MM_CONFIG_DOCTOOL_DIR])[]dnl
AC_REQUIRE([_MM_ARG_ENABLE_DOCUMENTATION])[]dnl
dnl
AS_IF([test "x$ENABLE_DOCUMENTATION" != xno],
      [_MM_ARG_WITH_TAGFILE_DOC(m4_quote(m4_bpatsubst([$1], [[+]*\([-+][0123456789]\|[._]\).*$])),
                                [htmlref]m4_ifval([$2], [[pub]], [[dir]]), [$1], [$2])])[]dnl
])
