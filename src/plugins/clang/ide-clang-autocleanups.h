/* ide-clang-autocleanups.h
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

#include <clang-c/Index.h>
#include <glib.h>

G_BEGIN_DECLS

static inline void
_ide_clang_dispose_string (CXString *str)
{
  if (str != NULL && str->data != NULL)
    clang_disposeString (*str);
}

static inline void
_ide_clang_dispose_diagnostic (CXDiagnostic *diag)
{
  if (diag != NULL)
    clang_disposeDiagnostic (diag);
}

static inline void
_ide_clang_dispose_index (CXIndex *idx)
{
  if (idx != NULL && *idx != NULL)
    clang_disposeIndex (*idx);
}

static inline void
_ide_clang_dispose_unit (CXTranslationUnit *unit)
{
  if (unit != NULL && *unit != NULL)
    clang_disposeTranslationUnit (*unit);
}

static inline void
_ide_clang_dispose_cursor (CXCursor *cursor)
{
  /* Only use when g_slice_dup()'ing cursor (which means you should
   * be using *cursor to dereference and pass to Clang API.
   */
  g_slice_free (CXCursor, cursor);
}

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (CXString,              _ide_clang_dispose_string)
G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (CXIndex,               _ide_clang_dispose_index)
G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (CXTranslationUnit,     _ide_clang_dispose_unit)
G_DEFINE_AUTOPTR_CLEANUP_FUNC    (CXDiagnostic,          _ide_clang_dispose_diagnostic)
G_DEFINE_AUTOPTR_CLEANUP_FUNC    (CXCursor,              _ide_clang_dispose_cursor)
G_DEFINE_AUTOPTR_CLEANUP_FUNC    (CXCodeCompleteResults, clang_disposeCodeCompleteResults)

G_END_DECLS
