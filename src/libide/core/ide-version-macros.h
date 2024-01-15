/* ide-version-macros.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib.h>

#include "ide-version.h"

#ifndef _IDE_EXTERN
# define _IDE_EXTERN extern
#endif

#define IDE_VERSION_CUR_STABLE (G_ENCODE_VERSION (IDE_MAJOR_VERSION, 0))

#ifdef IDE_DISABLE_DEPRECATION_WARNINGS
# define IDE_DEPRECATED _IDE_EXTERN
# define IDE_DEPRECATED_FOR(f) _IDE_EXTERN
# define IDE_UNAVAILABLE(maj,min) _IDE_EXTERN
#else
# define IDE_DEPRECATED G_DEPRECATED _IDE_EXTERN
# define IDE_DEPRECATED_FOR(f) G_DEPRECATED_FOR (f) _IDE_EXTERN
# define IDE_UNAVAILABLE(maj,min) G_UNAVAILABLE (maj, min) _IDE_EXTERN
#endif

#define IDE_VERSION_43 (G_ENCODE_VERSION (43, 0))
#define IDE_VERSION_44 (G_ENCODE_VERSION (44, 0))
#define IDE_VERSION_45 (G_ENCODE_VERSION (45, 0))
#define IDE_VERSION_46 (G_ENCODE_VERSION (46, 0))

#if IDE_MAJOR_VERSION == IDE_VERSION_43
# define IDE_VERSION_PREV_STABLE (IDE_VERSION_43)
#else
# define IDE_VERSION_PREV_STABLE (G_ENCODE_VERSION (IDE_MAJOR_VERSION - 1, 0))
#endif

/**
 * IDE_VERSION_MIN_REQUIRED:
 *
 * A macro that should be defined by the user prior to including
 * the ide.h header.
 *
 * The definition should be one of the predefined IDE version
 * macros: %IDE_VERSION_43, ...
 *
 * This macro defines the lower bound for the Builder API to use.
 *
 * If a function has been deprecated in a newer version of Builder,
 * it is possible to use this symbol to avoid the compiler warnings
 * without disabling warning for every deprecated function.
 */
#ifndef IDE_VERSION_MIN_REQUIRED
# define IDE_VERSION_MIN_REQUIRED (IDE_VERSION_CUR_STABLE)
#endif

/**
 * IDE_VERSION_MAX_ALLOWED:
 *
 * A macro that should be defined by the user prior to including
 * the ide.h header.

 * The definition should be one of the predefined Builder version
 * macros: %IDE_VERSION_43, %IDE_VERSION_44,...
 *
 * This macro defines the upper bound for the IDE API to use.
 *
 * If a function has been introduced in a newer version of Builder,
 * it is possible to use this symbol to get compiler warnings when
 * trying to use that function.
 */
#ifndef IDE_VERSION_MAX_ALLOWED
# if IDE_VERSION_MIN_REQUIRED > IDE_VERSION_PREV_STABLE
#  define IDE_VERSION_MAX_ALLOWED (IDE_VERSION_MIN_REQUIRED)
# else
#  define IDE_VERSION_MAX_ALLOWED (IDE_VERSION_CUR_STABLE)
# endif
#endif

#define IDE_AVAILABLE_IN_ALL _IDE_EXTERN

#if IDE_VERSION_MIN_REQUIRED >= IDE_VERSION_43
# define IDE_DEPRECATED_IN_43 IDE_DEPRECATED
# define IDE_DEPRECATED_IN_43_FOR(f) IDE_DEPRECATED_FOR(f)
#else
# define IDE_DEPRECATED_IN_43 _IDE_EXTERN
# define IDE_DEPRECATED_IN_43_FOR(f) _IDE_EXTERN
#endif
#if IDE_VERSION_MAX_ALLOWED < IDE_VERSION_43
# define IDE_AVAILABLE_IN_43 IDE_UNAVAILABLE(43, 0)
#else
# define IDE_AVAILABLE_IN_43 _IDE_EXTERN
#endif

#if IDE_VERSION_MIN_REQUIRED >= IDE_VERSION_44
# define IDE_DEPRECATED_IN_44 IDE_DEPRECATED
# define IDE_DEPRECATED_IN_44_FOR(f) IDE_DEPRECATED_FOR(f)
#else
# define IDE_DEPRECATED_IN_44 _IDE_EXTERN
# define IDE_DEPRECATED_IN_44_FOR(f) _IDE_EXTERN
#endif
#if IDE_VERSION_MAX_ALLOWED < IDE_VERSION_44
# define IDE_AVAILABLE_IN_44 IDE_UNAVAILABLE(44, 0)
#else
# define IDE_AVAILABLE_IN_44 _IDE_EXTERN
#endif

#if IDE_VERSION_MIN_REQUIRED >= IDE_VERSION_45
# define IDE_DEPRECATED_IN_45 IDE_DEPRECATED
# define IDE_DEPRECATED_IN_45_FOR(f) IDE_DEPRECATED_FOR(f)
#else
# define IDE_DEPRECATED_IN_45 _IDE_EXTERN
# define IDE_DEPRECATED_IN_45_FOR(f) _IDE_EXTERN
#endif
#if IDE_VERSION_MAX_ALLOWED < IDE_VERSION_45
# define IDE_AVAILABLE_IN_45 IDE_UNAVAILABLE(45, 0)
#else
# define IDE_AVAILABLE_IN_45 _IDE_EXTERN
#endif

#if IDE_VERSION_MIN_REQUIRED >= IDE_VERSION_46
# define IDE_DEPRECATED_IN_46 IDE_DEPRECATED
# define IDE_DEPRECATED_IN_46_FOR(f) IDE_DEPRECATED_FOR(f)
#else
# define IDE_DEPRECATED_IN_46 _IDE_EXTERN
# define IDE_DEPRECATED_IN_46_FOR(f) _IDE_EXTERN
#endif
#if IDE_VERSION_MAX_ALLOWED < IDE_VERSION_46
# define IDE_AVAILABLE_IN_46 IDE_UNAVAILABLE(46, 0)
#else
# define IDE_AVAILABLE_IN_46 _IDE_EXTERN
#endif
