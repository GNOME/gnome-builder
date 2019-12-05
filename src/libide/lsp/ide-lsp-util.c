/* ide-lsp-util.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#include "ide-lsp-util.h"

IdeSymbolKind
ide_lsp_decode_symbol_kind (guint kind)
{
  switch (kind)
    {
    case 1:   kind = IDE_SYMBOL_KIND_FILE;         break;
    case 2:   kind = IDE_SYMBOL_KIND_MODULE;       break;
    case 3:   kind = IDE_SYMBOL_KIND_NAMESPACE;    break;
    case 4:   kind = IDE_SYMBOL_KIND_PACKAGE;      break;
    case 5:   kind = IDE_SYMBOL_KIND_CLASS;        break;
    case 6:   kind = IDE_SYMBOL_KIND_METHOD;       break;
    case 7:   kind = IDE_SYMBOL_KIND_PROPERTY;     break;
    case 8:   kind = IDE_SYMBOL_KIND_FIELD;        break;
    case 9:   kind = IDE_SYMBOL_KIND_CONSTRUCTOR;  break;
    case 10:  kind = IDE_SYMBOL_KIND_ENUM;         break;
    case 11:  kind = IDE_SYMBOL_KIND_INTERFACE;    break;
    case 12:  kind = IDE_SYMBOL_KIND_FUNCTION;     break;
    case 13:  kind = IDE_SYMBOL_KIND_VARIABLE;     break;
    case 14:  kind = IDE_SYMBOL_KIND_CONSTANT;     break;
    case 15:  kind = IDE_SYMBOL_KIND_STRING;       break;
    case 16:  kind = IDE_SYMBOL_KIND_NUMBER;       break;
    case 17:  kind = IDE_SYMBOL_KIND_BOOLEAN;      break;
    case 18:  kind = IDE_SYMBOL_KIND_ARRAY;        break;
    case 19:  kind = IDE_SYMBOL_KIND_OBJECT;       break;
    case 20:  kind = IDE_SYMBOL_KIND_VARIABLE;     break; /* Key */
    case 21:  kind = IDE_SYMBOL_KIND_CONSTANT;     break; /* Null */
    case 22:  kind = IDE_SYMBOL_KIND_ENUM_VALUE;   break;
    case 23:  kind = IDE_SYMBOL_KIND_STRUCT;       break;
    case 24:  kind = IDE_SYMBOL_KIND_EVENT;        break;
    case 25:  kind = IDE_SYMBOL_KIND_OPERATOR;     break;
    case 26:  kind = IDE_SYMBOL_KIND_TYPE_PARAM;   break;
    default:  kind = IDE_SYMBOL_KIND_NONE;         break;
    }

  return kind;
}

IdeSymbolKind
ide_lsp_decode_completion_kind (guint kind)
{
  switch (kind)
    {
    case 1:   kind = IDE_SYMBOL_KIND_STRING;       break;
    case 2:   kind = IDE_SYMBOL_KIND_METHOD;       break;
    case 3:   kind = IDE_SYMBOL_KIND_FUNCTION;     break;
    case 4:   kind = IDE_SYMBOL_KIND_CONSTRUCTOR;  break;
    case 5:   kind = IDE_SYMBOL_KIND_FIELD;        break;
    case 6:   kind = IDE_SYMBOL_KIND_VARIABLE;     break;
    case 7:   kind = IDE_SYMBOL_KIND_CLASS;        break;
    case 8:   kind = IDE_SYMBOL_KIND_INTERFACE;    break;
    case 9:   kind = IDE_SYMBOL_KIND_MODULE;       break;
    case 10:  kind = IDE_SYMBOL_KIND_PROPERTY;     break;
    case 11:  kind = IDE_SYMBOL_KIND_NUMBER;       break;
    case 12:  kind = IDE_SYMBOL_KIND_SCALAR;       break;
    case 13:  kind = IDE_SYMBOL_KIND_ENUM_VALUE;   break;
    case 14:  kind = IDE_SYMBOL_KIND_KEYWORD;      break;
    case 17:  kind = IDE_SYMBOL_KIND_FILE;         break;

    case 15: /* Snippet */
    case 16: /* Color */
    case 18: /* Reference */
    default:  kind = IDE_SYMBOL_KIND_NONE;         break;
    }

  return kind;
}
