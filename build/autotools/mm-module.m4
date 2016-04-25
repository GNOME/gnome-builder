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

#serial 20091228

## _MM_INIT_MODULE_VERSION(basename, BASENAME, [major], [minor], [micro])
##
m4_define([_MM_INIT_MODULE_VERSION],
[dnl
m4_ifval([$3],
[AC_SUBST([$2][_MAJOR_VERSION], [$3])
AC_DEFINE([$2][_MAJOR_VERSION], [$3], [Major version number of $1.])
])[]dnl
m4_ifval([$4],
[AC_SUBST([$2][_MINOR_VERSION], [$4])
AC_DEFINE([$2][_MINOR_VERSION], [$4], [Minor version number of $1.])
])[]dnl
m4_ifval([$5],
[AC_SUBST([$2][_MICRO_VERSION], [$5])
AC_DEFINE([$2][_MICRO_VERSION], [$5], [Micro version number of $1.])
])[]dnl
])

## _MM_INIT_MODULE_SUBST(module-name, module-version, basename, api-version, BASENAME)
##
m4_define([_MM_INIT_MODULE_SUBST],
[dnl
AC_SUBST([$5][_MODULE_NAME], ['$1'])
AC_SUBST([$5][_VERSION], ['$2'])
m4_ifval([$4],
[AC_SUBST([$5][_API_VERSION], ['$4'])
])[]dnl
_MM_INIT_MODULE_VERSION([$3], [$5], m4_bpatsubst([$2], [[^0123456789]+], [,]))[]dnl
])

## _MM_INIT_MODULE_BASENAME(module-name, module-version, basename, api-version)
##
m4_define([_MM_INIT_MODULE_BASENAME],
          [_MM_INIT_MODULE_SUBST([$1], [$2], [$3], [$4],
                                 m4_quote(AS_TR_CPP(m4_quote(m4_translit([$3], [+], [X])))))])

## MM_INIT_MODULE(module-name, [module-version])
##
## Set up substitution variables and macro definitions for a module with
## the specified pkg-config <module-name> and <module-version> triplet.
## If no <module-version> is specified, it defaults to the expansion of
## AC_PACKAGE_VERSION.
##
## Substitutions: <BASENAME>_MODULE_NAME        <module-name>
##                <BASENAME>_VERSION            <module-version>
##                <BASENAME>_API_VERSION        <api-version>
##                <BASENAME>_MAJOR_VERSION      <major>
##                <BASENAME>_MINOR_VERSION      <minor>
##                <BASENAME>_MICRO_VERSION      <micro>
##
## Macro defines: <BASENAME>_MAJOR_VERSION      <major>
##                <BASENAME>_MINOR_VERSION      <minor>
##                <BASENAME>_MICRO_VERSION      <micro>
##
## Where:         <BASENAME>                    AS_TR_CPP(<basename> =~ tr/+/X/)
##                <basename>[-<api-version>]    <module-name>
##                <major>.<minor>.<micro>[.*]   <module-version>
##
AC_DEFUN([MM_INIT_MODULE],
[dnl
m4_assert([$# >= 1])[]dnl
AC_REQUIRE([_MM_PRE_INIT])[]dnl
AC_REQUIRE([MM_CHECK_GNU_MAKE])[]dnl
_MM_INIT_MODULE_BASENAME([$1],
                 m4_quote(m4_ifval([$2], [$2], m4_defn([AC_PACKAGE_VERSION]))),
                 m4_quote(m4_bpatsubst([$1], [[-.0123456789]+$])),
                 m4_quote(m4_bregexp([$1], [-?\([.0123456789]+\)$], [\1])))[]dnl
])
