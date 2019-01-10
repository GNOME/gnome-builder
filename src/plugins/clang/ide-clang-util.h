/* ide-clang-util.h
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

#include <libide-code.h>

#include "ide-clang-autocleanups.h"

G_BEGIN_DECLS

static inline IdeSymbolKind
ide_clang_translate_kind (enum CXCursorKind cursor_kind)
{
  switch ((int)cursor_kind)
    {
    case CXCursor_StructDecl:
      return IDE_SYMBOL_KIND_STRUCT;

    case CXCursor_UnionDecl:
      return IDE_SYMBOL_KIND_UNION;

    case CXCursor_ClassDecl:
      return IDE_SYMBOL_KIND_CLASS;

    case CXCursor_EnumDecl:
      return IDE_SYMBOL_KIND_ENUM;

    case CXCursor_FieldDecl:
      return IDE_SYMBOL_KIND_FIELD;

    case CXCursor_EnumConstantDecl:
      return IDE_SYMBOL_KIND_ENUM_VALUE;

    case CXCursor_FunctionDecl:
      return IDE_SYMBOL_KIND_FUNCTION;

    case CXCursor_CXXMethod:
      return IDE_SYMBOL_KIND_METHOD;

    case CXCursor_VarDecl:
    case CXCursor_ParmDecl:
      return IDE_SYMBOL_KIND_VARIABLE;

    case CXCursor_TypedefDecl:
    case CXCursor_NamespaceAlias:
    case CXCursor_TypeAliasDecl:
      return IDE_SYMBOL_KIND_ALIAS;

    case CXCursor_Namespace:
      return IDE_SYMBOL_KIND_NAMESPACE;

    case CXCursor_FunctionTemplate:
    case CXCursor_ClassTemplate:
      return IDE_SYMBOL_KIND_TEMPLATE;

    case CXCursor_MacroDefinition:
      return IDE_SYMBOL_KIND_MACRO;

    default:
      return IDE_SYMBOL_KIND_NONE;
    }
}

static inline IdeDiagnosticSeverity
ide_clang_translate_severity (enum CXDiagnosticSeverity severity)
{
  switch (severity)
    {
    case CXDiagnostic_Ignored:
      return IDE_DIAGNOSTIC_IGNORED;

    case CXDiagnostic_Note:
      return IDE_DIAGNOSTIC_NOTE;

    case CXDiagnostic_Warning:
      return IDE_DIAGNOSTIC_WARNING;

    case CXDiagnostic_Error:
      return IDE_DIAGNOSTIC_ERROR;

    case CXDiagnostic_Fatal:
      return IDE_DIAGNOSTIC_FATAL;

    default:
      return 0;
    }
}

G_END_DECLS
