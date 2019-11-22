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
#define _IDE_EXTERN extern
#endif

#ifdef IDE_DISABLE_DEPRECATION_WARNINGS
#define IDE_DEPRECATED _IDE_EXTERN
#define IDE_DEPRECATED_FOR(f) _IDE_EXTERN
#define IDE_UNAVAILABLE(maj,min) _IDE_EXTERN
#else
#define IDE_DEPRECATED G_DEPRECATED _IDE_EXTERN
#define IDE_DEPRECATED_FOR(f) G_DEPRECATED_FOR(f) _IDE_EXTERN
#define IDE_UNAVAILABLE(maj,min) G_UNAVAILABLE(maj,min) _IDE_EXTERN
#endif

#define IDE_VERSION_3_28 (G_ENCODE_VERSION (3, 28))
#define IDE_VERSION_3_30 (G_ENCODE_VERSION (3, 30))
#define IDE_VERSION_3_32 (G_ENCODE_VERSION (3, 32))
#define IDE_VERSION_3_34 (G_ENCODE_VERSION (3, 34))
#define IDE_VERSION_3_36 (G_ENCODE_VERSION (3, 36))

#if (IDE_MINOR_VERSION == 99)
# define IDE_VERSION_CUR_STABLE (G_ENCODE_VERSION (IDE_MAJOR_VERSION + 1, 0))
#elif (IDE_MINOR_VERSION % 2)
# define IDE_VERSION_CUR_STABLE (G_ENCODE_VERSION (IDE_MAJOR_VERSION, IDE_MINOR_VERSION + 1))
#else
# define IDE_VERSION_CUR_STABLE (G_ENCODE_VERSION (IDE_MAJOR_VERSION, IDE_MINOR_VERSION))
#endif

#if (IDE_MINOR_VERSION == 99)
# define IDE_VERSION_PREV_STABLE (G_ENCODE_VERSION (IDE_MAJOR_VERSION + 1, 0))
#elif (IDE_MINOR_VERSION % 2)
# define IDE_VERSION_PREV_STABLE (G_ENCODE_VERSION (IDE_MAJOR_VERSION, IDE_MINOR_VERSION - 1))
#else
# define IDE_VERSION_PREV_STABLE (G_ENCODE_VERSION (IDE_MAJOR_VERSION, IDE_MINOR_VERSION - 2))
#endif

/**
 * IDE_VERSION_MIN_REQUIRED:
 *
 * A macro that should be defined by the user prior to including
 * the ide.h header.
 *
 * The definition should be one of the predefined IDE version
 * macros: %IDE_VERSION_3_28, ...
 *
 * This macro defines the lower bound for the Builder API to use.
 *
 * If a function has been deprecated in a newer version of Builder,
 * it is possible to use this symbol to avoid the compiler warnings
 * without disabling warning for every deprecated function.
 *
 * Since: 3.32
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
 * macros: %IDE_VERSION_1_0, %IDE_VERSION_1_2,...
 *
 * This macro defines the upper bound for the IDE API to use.
 *
 * If a function has been introduced in a newer version of Builder,
 * it is possible to use this symbol to get compiler warnings when
 * trying to use that function.
 *
 * Since: 3.32
 */
#ifndef IDE_VERSION_MAX_ALLOWED
# if IDE_VERSION_MIN_REQUIRED > IDE_VERSION_PREV_STABLE
#  define IDE_VERSION_MAX_ALLOWED (IDE_VERSION_MIN_REQUIRED)
# else
#  define IDE_VERSION_MAX_ALLOWED (IDE_VERSION_CUR_STABLE)
# endif
#endif

#if IDE_VERSION_MAX_ALLOWED < IDE_VERSION_MIN_REQUIRED
#error "IDE_VERSION_MAX_ALLOWED must be >= IDE_VERSION_MIN_REQUIRED"
#endif
#if IDE_VERSION_MIN_REQUIRED < IDE_VERSION_3_28
#error "IDE_VERSION_MIN_REQUIRED must be >= IDE_VERSION_3_28"
#endif

#define IDE_AVAILABLE_IN_ALL                   _IDE_EXTERN

#if IDE_VERSION_MIN_REQUIRED >= IDE_VERSION_3_28
# define IDE_DEPRECATED_IN_3_28                IDE_DEPRECATED
# define IDE_DEPRECATED_IN_3_28_FOR(f)         IDE_DEPRECATED_FOR(f)
#else
# define IDE_DEPRECATED_IN_3_28                _IDE_EXTERN
# define IDE_DEPRECATED_IN_3_28_FOR(f)         _IDE_EXTERN
#endif

#if IDE_VERSION_MAX_ALLOWED < IDE_VERSION_3_28
# define IDE_AVAILABLE_IN_3_28                 IDE_UNAVAILABLE(3, 28)
#else
# define IDE_AVAILABLE_IN_3_28                 _IDE_EXTERN
#endif

#if IDE_VERSION_MIN_REQUIRED >= IDE_VERSION_3_30
# define IDE_DEPRECATED_IN_3_30                IDE_DEPRECATED
# define IDE_DEPRECATED_IN_3_30_FOR(f)         IDE_DEPRECATED_FOR(f)
#else
# define IDE_DEPRECATED_IN_3_30                _IDE_EXTERN
# define IDE_DEPRECATED_IN_3_30_FOR(f)         _IDE_EXTERN
#endif

#if IDE_VERSION_MAX_ALLOWED < IDE_VERSION_3_30
# define IDE_AVAILABLE_IN_3_30                 IDE_UNAVAILABLE(3, 30)
#else
# define IDE_AVAILABLE_IN_3_30                 _IDE_EXTERN
#endif

#if IDE_VERSION_MIN_REQUIRED >= IDE_VERSION_3_32
# define IDE_DEPRECATED_IN_3_32                IDE_DEPRECATED
# define IDE_DEPRECATED_IN_3_32_FOR(f)         IDE_DEPRECATED_FOR(f)
#else
# define IDE_DEPRECATED_IN_3_32                _IDE_EXTERN
# define IDE_DEPRECATED_IN_3_32_FOR(f)         _IDE_EXTERN
#endif

#if IDE_VERSION_MAX_ALLOWED < IDE_VERSION_3_32
# define IDE_AVAILABLE_IN_3_32                 IDE_UNAVAILABLE(3, 32)
#else
# define IDE_AVAILABLE_IN_3_32                 _IDE_EXTERN
#endif

#if IDE_VERSION_MIN_REQUIRED >= IDE_VERSION_3_34
# define IDE_DEPRECATED_IN_3_34                IDE_DEPRECATED
# define IDE_DEPRECATED_IN_3_34_FOR(f)         IDE_DEPRECATED_FOR(f)
#else
# define IDE_DEPRECATED_IN_3_34                _IDE_EXTERN
# define IDE_DEPRECATED_IN_3_34_FOR(f)         _IDE_EXTERN
#endif

#if IDE_VERSION_MAX_ALLOWED < IDE_VERSION_3_34
# define IDE_AVAILABLE_IN_3_34                 IDE_UNAVAILABLE(3, 34)
#else
# define IDE_AVAILABLE_IN_3_34                 _IDE_EXTERN
#endif

#if IDE_VERSION_MIN_REQUIRED >= IDE_VERSION_3_36
# define IDE_DEPRECATED_IN_3_36                IDE_DEPRECATED
# define IDE_DEPRECATED_IN_3_36_FOR(f)         IDE_DEPRECATED_FOR(f)
#else
# define IDE_DEPRECATED_IN_3_36                _IDE_EXTERN
# define IDE_DEPRECATED_IN_3_36_FOR(f)         _IDE_EXTERN
#endif

#if IDE_VERSION_MAX_ALLOWED < IDE_VERSION_3_36
# define IDE_AVAILABLE_IN_3_36                 IDE_UNAVAILABLE(3, 36)
#else
# define IDE_AVAILABLE_IN_3_36                 _IDE_EXTERN
#endif
